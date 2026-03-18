#pragma once
#include <functional>
#include <string>
#include <sys/uio.h>
#include "dispatcher/dispatcher.h"
#include "channel/channel.h"
#include "config/config.h"
#include "buffer/buffer.h"

enum class LineStatus
{
    Line_OK,
    Line_Bad,
    Line_Open // 表示这一次数据没读完
};

// 解析状态
enum class ParseState : char
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
    InternelError, // 内部错误
    CLOSED_CONNECTION
};

class HttpConnection
{
public:
    using CloseCallback = std::function<void(int)>;

    HttpConnection(const Config &config, int fd, Dispatcher *dispatcher, CloseCallback closeCallback);
    ~HttpConnection();

    void CallbackProcessRead();
    void CallbackProcessWrite();
    void CallbackProcessClose();
    int LTRead();
    int ETRead();
    int LTWrite();
    int ETWrite();

private:
    // 解析http请求并准备响应消息
    HttpCode process_read();
    // 根据返回值来准备要发送的数据
    HttpCode process_write(HttpCode ret);

    // 取出buffer中的一行
    LineStatus get_one_line(std::string &oneLine);

    // 解析请求行，获得请求方法、目标url、http版本号
    HttpCode parse_request_line(std::string &s);

    // 解析请求头
    HttpCode parse_request_headers(std::string &s);

    // 解析请求体
    HttpCode parse_request_content();

    // 关闭httpConn连接并释放对应的数据
    void closeConnection();

    Dispatcher *m_dispatcher;
    Channel *m_channel;
    CloseCallback m_closeCallback; // 销毁当前httpConn的回调
    Buffer m_readBuf;
    Buffer m_writeBuf;
    Config m_config;
    bool is_use_log;
    bool m_readClosed;          // 用来标记对端是否关闭
    ParseState m_curParseState; // 解析http请求状态

    Method m_method;       // 请求行-请求方法
    std::string m_url;     // 请求行-请求地址
    std::string m_version; // 请求行信息-请求版本
    int m_content_length;  // 请求头-数据块长度
    bool m_alive;          // 请求头-连接是否保活

    std::string m_body; // 请求体-数据
};
