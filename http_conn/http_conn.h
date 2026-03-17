#pragma once
#include <functional>
#include <string>
#include <sys/uio.h>
#include "dispatcher/dispatcher.h"
#include "channel/channel.h"
#include "config/config.h"
#include "buffer/buffer.h"

class HttpConnection
{
public:
    using CloseCallback = std::function<void(int)>;

    HttpConnection(const Config &config, int fd, Dispatcher *dispatcher, CloseCallback closeCallback);
    ~HttpConnection();

    void processRead();
    void processWrite();
    void processClose();
    int LTRead();
    int ETRead();

private:
    void initResponse();
    void closeConnection();

    Dispatcher *m_dispatcher;
    Channel *m_channel;
    CloseCallback m_closeCallback;  // 销毁当前httpConn的回调
    Buffer m_readBuf;
    Buffer m_writeBuf;
    Config m_config;
    bool is_use_log;
};
