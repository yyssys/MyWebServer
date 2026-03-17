#pragma once
#include <sys/uio.h>
#include "dispatcher/dispatcher.h"
#include "channel/channel.h"
#include "config/config.h"
#include "buffer/buffer.h"

class HttpConnection
{
public:
    HttpConnection(const Config &config, int fd, Dispatcher *dispatcher);
    ~HttpConnection() = default;

    int processRead();
    int processWrite();
    int LTRead();
    int ETRead();

private:
    Dispatcher *m_dispatcher;
    Channel *m_channel;
    Buffer m_readBuf;
    Buffer m_writeBuf;
    Config m_config;
    bool is_use_log;
};