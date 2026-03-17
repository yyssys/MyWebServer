#include "http_conn.h"

HttpConnection::HttpConnection(const Config &config, int fd, Dispatcher *dispatcher)
    : m_config(config), m_dispatcher(dispatcher), is_use_log(config.enableLogging)
{
    m_channel = new Channel(fd, FDEvent::ReadEvent, std::bind(&HttpConnection::processRead, this), nullptr);
    m_dispatcher->add(m_channel);
}

int HttpConnection::processRead()
{
    switch (m_config.triggerMode)
    {
    case TriggerMode::LevelTriggered:

        break;
    case TriggerMode::EdgeTriggered:
        break;
    }
    return 0;
}

int HttpConnection::LTRead()
{
    struct iovec vec[2];
    // 初始化数组元素
    int writeable = m_readBuf.writeAbleSize();
    vec[0].iov_base = m_readBuf.writeStartPos();
    vec[0].iov_len = writeable;
    // 再创建 4KB 的缓冲区来存数据
    char *tmpbuf = (char *)malloc(40960);
    vec[1].iov_base = tmpbuf;
    vec[1].iov_len = 40960;
    int result = readv(m_channel->getFd(), vec, 2);
    if (result == -1)
    {
        LOG_ERROR("连接出错，关闭连接");
        m_dispatcher->remove(m_channel);
        m_channel = nullptr;
    }
    else if (result == 0)
    {
        // 对端关闭
        LOG_INFO("关闭与{}的连接", m_channel->getFd());
        m_dispatcher->remove(m_channel);
        m_channel = nullptr;
    }
    else if (result <= writeable)
    {
        m_readBuf.updateWritePos(result);
    }
    else
    {
       
    }
    free(tmpbuf);
    return result;
}
