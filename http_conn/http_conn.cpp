#include "http_conn.h"
#include <cppconn/resultset.h>
#include <cerrno>
#include <cctype>
#include <cstring>
#include <fstream>
#include <sstream>
#include <utility>

// 定义http响应的一些状态信息
const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";

std::string getFileType(const std::string &path)
{
    const int pos = path.rfind('.');
    const std::string ext = pos == std::string::npos ? "" : path.substr(pos);
    if (ext == ".html" || ext == ".htm")
        return "text/html; charset=utf-8";
    if (ext == ".css")
        return "text/css; charset=utf-8";
    if (ext == ".js")
        return "application/javascript; charset=utf-8";
    if (ext == ".json")
        return "application/json; charset=utf-8";
    if (ext == ".png")
        return "image/png";
    if (ext == ".jpg" || ext == ".jpeg")
        return "image/jpeg";
    if (ext == ".gif")
        return "image/gif";
    if (ext == ".svg")
        return "image/svg+xml";
    return "text/plain; charset=utf-8";
}

HttpConnection::HttpConnection(const Config &config, int fd, Dispatcher *dispatcher, CloseCallback closeCallback)
    : m_dispatcher(dispatcher),
      m_channel(nullptr),
      m_closeCallback(std::move(closeCallback)),
      m_config(config),
      is_use_log(config.enableLogging),
      m_readClosed(false),
      m_curParseState(ParseState::ParseReqLine),
      m_content_length(0),
      m_alive(false),
      m_mmap_address(nullptr)
{
    m_channel = new Channel(
        fd,
        FDEvent::ReadEvent,
        std::bind(&HttpConnection::CallbackProcessRead, this),
        std::bind(&HttpConnection::CallbackProcessWrite, this),
        std::bind(&HttpConnection::CallbackProcessClose, this));
    m_dispatcher->add(m_channel);
}

HttpConnection::~HttpConnection()
{
    delete m_channel;
}

void HttpConnection::CallbackProcessRead()
{
    const int result = m_config.triggerMode == TriggerMode::LT ? LTRead() : ETRead();
    if (result < 0)
    {
        closeConnection();
        return;
    }
    // 读取到的数据已经处理完了
    if (m_readBuf.readAbleSize() <= 0)
    {
        // 对端关闭写，且服务器也没有要发送的数据了
        if (m_readClosed && m_writeBuf.readAbleSize() == 0)
        {
            closeConnection();
        }
        return;
    }
    // 解析http请求并准备响应消息
    HttpCode read_ret = process_read();
    if (read_ret == HttpCode::ReqIncomplete)
    {
        // 请求还没解析完，直接返回
        return;
    }
    if (!process_write(read_ret))
    {
        closeConnection();
    }
    else
    {
        // 检测写事件
        m_channel->setWriteEnabled(true);
        m_dispatcher->modify(m_channel);
    }
}

void HttpConnection::CallbackProcessWrite()
{
    if (m_writeBuf.readAbleSize() <= 0)
    {
        // 不再检测写事件
        m_channel->setWriteEnabled(false);
        m_dispatcher->modify(m_channel);
        init();
        return;
    }
    // 总共还需要发送的字节
    int bytes_to_send = m_writeBuf.readAbleSize() + m_file_stat.st_size;
    // 已经发送的字节
    int bytes_have_send = 0;
    while (1)
    {
        const int count = writev(m_channel->getFd(), m_iv, m_iv_count);

        if (count < 0)
        {
            // 发送缓冲区满了，返回等待写一次可写事件发生
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                m_channel->setWriteEnabled(true);
                m_dispatcher->modify(m_channel);
                return;
            }
            // 发送失败
            unmap();
            closeConnection();
            return;
        }
        else if (count == 0)
        {
            break;
        }
        bytes_have_send += count;
        bytes_to_send -= count;
        if (count >= m_writeBuf.readAbleSize())
        {
            m_writeBuf.updateReadPos(m_writeBuf.readAbleSize());
            m_iv[0].iov_len = 0;
            m_iv[1].iov_base = m_mmap_address + (bytes_have_send - m_writeBuf.readAbleSize());
            m_iv[1].iov_len = bytes_to_send;
        }
        else
        {
            m_writeBuf.updateReadPos(count);
            m_iv[0].iov_base = m_readBuf.data + m_readBuf.readPos;
            m_iv[0].iov_len -= bytes_have_send;
        }
        if (bytes_to_send <= 0)
        {
            // 发送完了，不再检测写事件
            m_channel->setWriteEnabled(false);
            m_dispatcher->modify(m_channel);

            if (m_alive)
            {
                init();
                return;
            }
            // 连接不保活则在一次请求响应后断开连接
            else
            {
                closeConnection();
            }
        }
    }
}

void HttpConnection::CallbackProcessClose()
{
    closeConnection();
}

int HttpConnection::LTRead()
{
    struct iovec vec[2];
    const int writable = m_readBuf.writeAbleSize();
    vec[0].iov_base = m_readBuf.data + m_readBuf.writePos;
    vec[0].iov_len = writable;
    char extraBuf[40960];
    vec[1].iov_base = extraBuf;
    vec[1].iov_len = sizeof(extraBuf);

    const int result = readv(m_channel->getFd(), vec, 2);
    if (result > 0)
    {
        if (result <= writable)
        {
            m_readBuf.updateWritePos(result);
        }
        else
        {
            m_readBuf.updateWritePos(writable);
            m_readBuf.appendData(extraBuf, result - writable);
        }
        return result;
    }
    if (result == 0)
    {
        m_readClosed = true;
        LOG_INFO("对端关闭写端, fd: {}", m_channel->getFd());
        return 0;
    }
    // 非阻塞模式读空缓冲区
    if (errno == EAGAIN || errno == EWOULDBLOCK)
    {
        return 0;
    }
    LOG_ERROR("readv failed, fd: {}, msg: {}", m_channel->getFd(), std::strerror(errno));
    return -1;
}

int HttpConnection::ETRead()
{
    int total = 0;
    while (true)
    {
        const int count = LTRead();
        if (count > 0)
        {
            total += count;
            continue;
        }
        if (count == 0)
        {
            return total;
        }
        return count;
    }
}

void HttpConnection::init()
{
    m_readBuf.retrieveAll();
    m_writeBuf.retrieveAll();
    m_curParseState = ParseState::ParseReqLine;
    m_method = Method::GET;
    m_url.clear();
    m_version.clear();
    m_content_length = 0;
    m_alive = false;
    m_body.clear();
}

HttpCode HttpConnection::process_read()
{
    HttpCode ret = HttpCode::ReqIncomplete;
    LineStatus line_status = LineStatus::Line_OK;
    std::string oneLine;
    oneLine.reserve(200);
    while ((line_status = get_one_line(oneLine)) == LineStatus::Line_OK)
    {
        LOG_INFO("HttpReq: {}", oneLine);
        switch (m_curParseState)
        {
        case ParseState::ParseReqLine:
        {
            ret = parse_request_line(oneLine);
            if (ret == HttpCode::ReqError)
                return HttpCode::ReqError;
            break;
        }
        case ParseState::ParseReqHeaders:
        {
            ret = parse_request_headers(oneLine);
            if (ret == HttpCode::ReqError)
                return HttpCode::ReqError;
            else if (ret == HttpCode::ReqComplete)
                return prepareResponse();
            break;
        }
        }
        // 请求行、请求头按行解析；请求体按长度解析
        if (m_curParseState == ParseState::ParseReqContent)
        {
            if (parse_request_content() == HttpCode::ReqComplete)
                return prepareResponse();
            break;
        }
    }

    // http请求格式有误
    if (line_status == LineStatus::Line_Bad)
        return HttpCode::ReqError;
    // 只接收到http请求的一部分数据
    return HttpCode::ReqIncomplete;
}

HttpCode HttpConnection::parse_request_line(std::string &s)
{
    // 找第一个空格：取方法
    int pos1 = s.find(' ');
    if (pos1 == std::string::npos)
        return HttpCode::ReqError;
    std::string method = s.substr(0, pos1);
    if (method == "GET")
        m_method = Method::GET;
    else if (method == "POST")
        m_method = Method::POST;
    else
        return HttpCode::ReqError;

    // 找第二个空格：取url
    int pos2 = s.find(' ', pos1 + 1);
    if (pos2 == std::string::npos)
        return HttpCode::ReqError;
    std::string url = s.substr(pos1 + 1, pos2 - pos1 - 1);
    if (strncasecmp(url.c_str(), "http://", 7) == 0)
    {
        url = url.substr(7);
        int pos = url.find('/');
        if (pos != std::string::npos)
        {
            url = url.substr(pos);
        }
    }
    if (strncasecmp(url.c_str(), "https://", 8) == 0)
    {
        url = url.substr(8);
        int pos = url.find('/');
        if (pos != std::string::npos)
        {
            url = url.substr(pos);
        }
    }
    m_url = url;
    // 请求必须以'/'开始
    if (m_url.size() == 0 || m_url[0] != '/')
        return HttpCode::ReqError;
    if (m_url.size() == 1)
    {
        // 如果访问的是根目录，则登录
        m_url += "login.html";
    }
    // 取版本号
    m_version = s.substr(pos2 + 1);
    if (strcasecmp(m_version.c_str(), "HTTP/1.1") != 0)
        return HttpCode::ReqError;

    // 下一步解析请求头
    m_curParseState = ParseState::ParseReqHeaders;
    // 请求解析还未完成
    return HttpCode::ReqIncomplete;
}

HttpCode HttpConnection::parse_request_headers(std::string &s)
{
    // 当前行是空行
    if (s.size() == 0)
    {
        // 请求体的数据长度不为0，则是一个POST请求
        if (m_content_length != 0)
        {
            // 下一步要解析数据块
            m_curParseState = ParseState::ParseReqContent;
            // 请求解析还未完成
            return HttpCode::ReqIncomplete;
        }
        else
        {
            // 这是一个GET请求,且解析请求完成
            return HttpCode::ReqComplete;
        }
    }
    // 解析请求头-数据块长度
    else if (strncasecmp(s.c_str(), "Content-length:", 15) == 0)
    {
        // 跳过 "Content-length:"
        int pos = 15;
        // 跳过后面的空格、制表符 \t
        while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t'))
        {
            pos++;
        }
        // 转成数字
        m_content_length = atoi(s.substr(pos).c_str());
    }
    // 解析请求头-连接保活
    else if (strncasecmp(s.c_str(), "Connection:", 11) == 0)
    {
        // 跳过 "Connection:"
        int pos = 11;
        // 跳过后面的空格、制表符 \t
        while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t'))
        {
            pos++;
        }
        if (strcasecmp(s.substr(pos).c_str(), "keep-alive") == 0)
            m_alive = true;
    }
    return HttpCode::ReqIncomplete;
}

HttpCode HttpConnection::parse_request_content()
{
    // 数据块已经全部接收到
    if (m_readBuf.readAbleSize() >= m_content_length)
    {
        m_body.assign(m_readBuf.data + m_readBuf.readPos, m_content_length);
        // 更新读指针
        m_readBuf.updateReadPos(m_content_length);
        return HttpCode::ReqComplete;
    }
    return HttpCode::ReqIncomplete;
}

HttpCode HttpConnection::prepareResponse()
{
    m_soucePath = m_config.rootPath;

    if (m_method == Method::POST && (m_url == "/2CGISQL.cgi" || m_url == "/3CGISQL.cgi"))
    {
        const auto form = parseFormUrlEncoded(m_body);
        const auto nameIter = form.find("user");
        const auto passwordIter = form.find("password");
        const std::string name = nameIter == form.end() ? "" : nameIter->second;
        const std::string passwd = passwordIter == form.end() ? "" : passwordIter->second;

        std::string targetFile = "/login.html";
        try
        {
            std::shared_ptr<sql::Connection> conn = MysqlConnPool::getInstance()->acquire();
            if (!conn)
            {
                return HttpCode::InternelError;
            }
            // 登录检测
            if (m_url == "/2CGISQL.cgi")
            {
                std::unique_ptr<sql::PreparedStatement> pstmt(
                    conn->prepareStatement("SELECT passwd FROM user WHERE username = ?"));
                pstmt->setString(1, name);
                std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
                if (res->next() && passwd == res->getString("passwd"))
                {
                    targetFile = "/index.html";
                }
                else
                {
                    targetFile = "/loginError.html";
                }
            }
            // 注册检测
            else
            {
                std::unique_ptr<sql::PreparedStatement> checkStmt(
                    conn->prepareStatement("SELECT 1 FROM user WHERE username = ?"));
                checkStmt->setString(1, name);
                std::unique_ptr<sql::ResultSet> res(checkStmt->executeQuery());

                if (res->next())
                {
                    targetFile = "/registerError.html";
                }
                else
                {
                    std::unique_ptr<sql::PreparedStatement> insertStmt(
                        conn->prepareStatement("INSERT INTO user(username, passwd) VALUES(?, ?)"));
                    insertStmt->setString(1, name);
                    insertStmt->setString(2, passwd);
                    insertStmt->executeUpdate();
                    targetFile = "/login.html";
                }
            }
        }
        catch (const std::exception &e)
        {
            LOG_ERROR("sql execute failed: {}", e.what());
            return HttpCode::InternelError;
        }

        m_soucePath += targetFile;
    }
    else
    {
        m_soucePath += m_url;
    }
    // 获取文件状态
    if (stat(m_soucePath.data(), &m_file_stat) < 0)
        return HttpCode::ReqNoResource;
    // 不能访问目录
    if (S_ISDIR(m_file_stat.st_mode))
        return HttpCode::ReqError;
    // 没有权限访问
    if (!(m_file_stat.st_mode & S_IROTH))
        return HttpCode::ReqForbidden;
    int fd = open(m_soucePath.c_str(), O_RDONLY);
    m_mmap_address = (char *)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    LOG_PRINT("访问资源：{}", m_soucePath);
    return HttpCode::ReqFile;
}

bool HttpConnection::process_write(HttpCode ret)
{
    switch (ret)
    {
    case HttpCode::InternelError:
        add_status_line(500, error_500_title);
        add_headers(strlen(error_500_form));
        add_content(error_500_form);
        break;
    case HttpCode::ReqError:
        add_status_line(400, error_400_title);
        add_headers(strlen(error_400_form));
        add_content(error_400_form);
        break;
    case HttpCode::ReqForbidden:
        add_status_line(403, error_403_title);
        add_headers(strlen(error_403_form));
        add_content(error_403_form);
        break;
    case HttpCode::ReqFile:
        add_status_line(200, ok_200_title);
        // 发送文件用mmap映射
        if (m_file_stat.st_size != 0)
        {
            add_headers(m_file_stat.st_size);
            m_iv[0].iov_base = m_writeBuf.data + m_writeBuf.readPos;
            m_iv[0].iov_len = m_writeBuf.readAbleSize();
            m_iv[1].iov_base = m_mmap_address;
            m_iv[1].iov_len = m_file_stat.st_size;
            m_iv_count = 2;
            return true;
        }
        else
        {
            const char *ok_string = "<html><body></body></html>";
            add_headers(strlen(ok_string));
            add_content(ok_string);
        }
    default:
        return false;
    }
    m_iv[0].iov_base = m_writeBuf.data + m_writeBuf.readPos;
    ;
    m_iv[0].iov_len = m_writeBuf.readAbleSize();
    m_iv_count = 1;

    return true;
}

std::string HttpConnection::urlDecode(const std::string &value) const
{
    std::string decoded;
    decoded.reserve(value.size());
    for (int i = 0; i < value.size(); ++i)
    {
        if (value[i] == '+')
        {
            decoded.push_back(' ');
            continue;
        }
        if (value[i] == '%' && i + 2 < value.size() &&
            std::isxdigit(static_cast<unsigned char>(value[i + 1])) &&
            std::isxdigit(static_cast<unsigned char>(value[i + 2])))
        {
            const std::string hex = value.substr(i + 1, 2);
            decoded.push_back(static_cast<char>(std::strtol(hex.c_str(), nullptr, 16)));
            i += 2;
            continue;
        }
        decoded.push_back(value[i]);
    }
    return decoded;
}

std::unordered_map<std::string, std::string> HttpConnection::parseFormUrlEncoded(const std::string &body) const
{
    std::unordered_map<std::string, std::string> form;
    int start = 0;
    while (start < body.size())
    {
        const int end = body.find('&', start);
        const std::string pair = body.substr(start, end == std::string::npos ? body.size() - start : end - start);
        const int eq = pair.find('=');
        if (eq != std::string::npos)
        {
            const std::string key = urlDecode(pair.substr(0, eq));
            const std::string value = urlDecode(pair.substr(eq + 1));
            form[key] = value;
        }
        if (end == std::string::npos)
        {
            break;
        }
        start = end + 1;
    }
    return form;
}

void HttpConnection::add_status_line(int status, std::string msg)
{
    add_response("{} {} {}\r\n", "HTTP/1.1", status, msg);
}

void HttpConnection::add_headers(int len)
{
    add_content_length(len);
    add_keep_alive();
    add_content_type();
    add_blank_line();
}

void HttpConnection::add_content_length(int len)
{
    add_response("Content-Length:{}\r\n", len);
}

void HttpConnection::add_keep_alive()
{
    add_response("Connection:{}\r\n", (m_alive == true) ? "keep-alive" : "close");
}

void HttpConnection::add_content_type()
{
    add_response("Content-Type:{}\r\n", getFileType(m_soucePath));
}

void HttpConnection::add_blank_line()
{
    add_response("{}", "\r\n");
}

void HttpConnection::add_content(const char *content)
{
    add_response("{}", content);
}

void HttpConnection::unmap()
{
    if (m_mmap_address)
    {
        munmap(m_mmap_address, m_file_stat.st_size);
        m_mmap_address = nullptr;
    }
}

void HttpConnection::closeConnection()
{
    if (m_channel == nullptr)
    {
        return;
    }

    const int fd = m_channel->getFd();
    Channel *channel = m_channel;
    m_channel = nullptr;
    m_dispatcher->remove(channel);
    delete channel;
    if (m_closeCallback)
    {
        m_closeCallback(fd);
    }
}

LineStatus HttpConnection::get_one_line(std::string &oneLine)
{
    char *const lineStart = m_readBuf.data + m_readBuf.readPos;
    char *const crlf = m_readBuf.findCRLF();
    if (crlf != nullptr)
    {
        oneLine.assign(lineStart, crlf - lineStart);
        // 修改读指针并跳过\r\n
        m_readBuf.updateReadPos(crlf - lineStart + 2);
        return LineStatus::Line_OK;
    }

    for (int i = m_readBuf.readPos; i < m_readBuf.writePos; ++i)
    {
        if (m_readBuf.data[i] == '\n')
        {
            return LineStatus::Line_Bad;
        }
    }
    // 所有读数据都读完都没找到\r\n,说明只接收http请求的一部分数据
    return LineStatus::Line_Open;
}
