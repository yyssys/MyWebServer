#pragma once
#include <memory>
#include <functional>
#include <unordered_map>
#include <sys/stat.h>
#include <string>
#include <sys/uio.h>
#include "dispatcher/dispatcher.h"
#include "channel/channel.h"
#include "config/config.h"
#include "buffer/buffer.h"
#include "sql_conn_pool/sql_conn_pool.h"
#include <sys/mman.h>
#include <fmt/core.h>

enum class LineStatus
{
    Line_OK,
    Line_Bad,
    Line_Open // 表示这一次数据没读完
};

// 解析状态
enum class ParseState
{
    ParseReqLine,
    ParseReqHeaders,
    ParseReqContent,
    ParseReqDone
};

// 请求方法
enum class Method
{
    GET,
    POST,
    HEAD,
    PUT,
    DELETE,
    TRACE,
    OPTIONS,
    CONNECT,
    PATH
};

enum class HttpCode
{
    ReqIncomplete, // 还不是一个完整的http请求
    ReqError,      // 请求格式错误
    ReqComplete,   // 一条http请求完成
    ReqNoResource, // 请求的资源不存在
    ReqForbidden,  // 请求的资源无权访问
    ReqFile,       // 请求文件
    ReqRedirect,   // 重定向
    InternelError  // 内部错误
};

class HttpConnection : public std::enable_shared_from_this<HttpConnection>
{
public:
    using CloseCallback = std::function<void(int)>;

    HttpConnection(const Config &config, int fd, Dispatcher *dispatcher, CloseCallback closeCallback);
    ~HttpConnection();

    void CallbackProcessRead();
    void CallbackProcessWrite();
    void CallbackProcessClose();
    void add();
    int LTRead();
    int ETRead();

private:
    void init();
    // 解析http请求并准备响应消息
    HttpCode process_read();
    // 根据返回值来准备要发送的数据
    bool process_write(HttpCode ret);

    // 取出buffer中的一行
    LineStatus get_one_line(std::string &oneLine);

    // 解析请求行，获得请求方法、目标url、http版本号
    HttpCode parse_request_line(std::string &s);

    // 解析请求头
    HttpCode parse_request_headers(std::string &s);

    // 解析请求体
    HttpCode parse_request_content();

    // 准备响应的数据
    HttpCode prepareResponse();
    // 字符解码
    std::string urlDecode(const std::string &value) const;
    // 解析出用户名和密码
    std::unordered_map<std::string, std::string> parseFormUrlEncoded(const std::string &body) const;
    // 添加状态行
    void add_status_line(int status, std::string msg);
    // 添加头
    void add_headers(int len);
    void add_content_length(int len);
    void add_location(const std::string &location);
    void add_keep_alive();
    void add_content_type();
    void add_blank_line();
    // 添加数据块
    void add_content(const char *content);

    template <class... Args>
    void add_response(const std::string &format, Args &&...args);

    // 关闭打开的mmap映射
    void unmap();

    // 关闭httpConn连接并释放对应的数据
    void closeConnection();

private:
    int m_fd;
    Dispatcher *m_dispatcher;
    std::unique_ptr<Channel> m_channel;
    CloseCallback m_closeCallback; // 销毁当前httpConn的回调
    Buffer m_readBuf;
    Buffer m_writeBuf;
    Config m_config;
    bool is_use_log;
    bool m_readClosed;          // 用来标记对端是否关闭
    ParseState m_curParseState; // 解析http请求状态
    std::string m_soucePath;    // 资源路径

    Method m_method;       // 请求行-请求方法
    std::string m_url;     // 请求行-请求地址
    std::string m_version; // 请求行信息-请求版本
    int m_content_length;  // 请求头-数据块长度
    bool m_alive;          // 请求头-连接是否保活

    std::string m_body; // 请求体-数据
    std::string m_redirectLocation;

    struct stat m_file_stat;
    char *m_mmap_address;

    struct iovec m_iv[2];
    int m_iv_count;
};

template <class... Args>
inline void HttpConnection::add_response(const std::string &format, Args &&...args)
{
    m_writeBuf.appendData(fmt::format(format, (args)...));
}
