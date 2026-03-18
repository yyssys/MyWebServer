#include "http_conn.h"
#include <cerrno>
#include <cstring>
#include <utility>

HttpConnection::HttpConnection(const Config &config, int fd, Dispatcher *dispatcher, CloseCallback closeCallback)
    : m_dispatcher(dispatcher),
      m_channel(nullptr),
      m_closeCallback(std::move(closeCallback)),
      m_config(config),
      is_use_log(config.enableLogging),
      m_readClosed(false),
      m_curParseState(ParseState::ParseReqLine),
      m_content_length(0),
      m_alive(false)
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
    if (result < 0 || m_channel == nullptr)
    {
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
    process_write(read_ret);
}

void HttpConnection::CallbackProcessWrite()
{
    if (m_config.triggerMode == TriggerMode::LT)
    {
        LTWrite();
        return;
    }
    ETWrite();
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
            m_readBuf.writePos += result;
        }
        else
        {
            m_readBuf.writePos = m_readBuf.capacity;
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
    closeConnection();
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

// int HttpConnection::LTWrite()
// {
//     const int result = m_writeBuf.sendData(m_channel->getFd());
//     if (result > 0)
//     {
//         if (m_writeBuf.readAbleSize() == 0)
//         {
//             closeConnection();
//         }
//         return result;
//     }

//     if (result == 0)
//     {
//         if (m_writeBuf.readAbleSize() == 0)
//         {
//             closeConnection();
//         }
//         return 0;
//     }

//     if (errno == EINTR)
//     {
//         return 0;
//     }
//     if (errno == EAGAIN || errno == EWOULDBLOCK)
//     {
//         enableWriteEvent();
//         return 0;
//     }

//     if (is_use_log)
//     {
//         LOG_ERROR("send failed, fd: {}, errno: {}", m_channel->getFd(), errno);
//     }
//     closeConnection();
//     return -1;
// }

// int HttpConnection::ETWrite()
// {
//     int total = 0;
//     while (m_writeBuf.readAbleSize() > 0)
//     {
//         const int count = m_writeBuf.sendData(m_channel->getFd());
//         if (count > 0)
//         {
//             total += count;
//             continue;
//         }
//         if (count == 0)
//         {
//             break;
//         }
//         if (errno == EINTR)
//         {
//             continue;
//         }
//         if (errno == EAGAIN || errno == EWOULDBLOCK)
//         {
//             enableWriteEvent();
//             return total;
//         }

//         if (is_use_log)
//         {
//             LOG_ERROR("send failed, fd: {}, errno: {}", m_channel->getFd(), errno);
//         }
//         closeConnection();
//         return -1;
//     }

//     closeConnection();
//     return total;
// }

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
                // return doRequest();
            break;
        }
        }
        // 请求行、请求头按行解析；请求体按长度解析
        if (m_curParseState == ParseState::ParseReqContent)
        {
            if (parse_request_content() == HttpCode::ReqComplete)
            {
                // return doRequest();
            }
            break;
        }
    }

    // http请求格式有误
    if (line_status == LineStatus::Line_Bad)
        return HttpCode::ReqError;
    // 只接收http请求的一部分数据
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
        url.substr(7);
        size_t pos = m_url.find('/'); // 找到第一个 '/' 的位置
        if (pos != std::string::npos)
        { // 如果找到了
            m_url = m_url.substr(pos);
        }
    }
    if (strncasecmp(url.c_str(), "https://", 8) == 0)
    {
        url.substr(8);
        size_t pos = m_url.find('/'); // 找到第一个 '/' 的位置
        if (pos != std::string::npos)
        { // 如果找到了
            m_url = m_url.substr(pos);
        }
    }
    if (m_url.size() == 0 || m_url[0] != '/')
        return HttpCode::ReqError;
    if (m_url.size() == 1)
    {
        // 如果访问的是根目录，则返回首页
        m_url += "index.html";
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
        size_t pos = 15;
        // 跳过后面的空格、制表符 \t
        while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t'))
        {
            pos++;
        }
        // 转成 long 数字
        m_content_length = atoi(s.substr(pos).c_str());
    }
    // 解析请求头-连接保活
    else if (strncasecmp(s.c_str(), "Connection:", 11) == 0)
    {
        // 跳过 "Connection:"
        size_t pos = 11;
        // 跳过后面的空格、制表符 \t
        while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t'))
        {
            pos++;
        }
        if (strcasecmp(s.substr(pos).c_str(), "keep-alive") == 0)
            m_alive = true;
    }
    // 解析请求头-连接保活
    else if (strncasecmp(s.c_str(), "Connection:", 11) == 0)
    {
        // 跳过 "Connection:"
        size_t pos = 11;
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
    if (m_readBuf.readAbleSize() >= m_content_length)
    {
        m_body.assign(m_readBuf.data + m_readBuf.readPos, m_content_length);
        return HttpCode::ReqComplete;
    }
    return HttpCode::ReqIncomplete;
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
