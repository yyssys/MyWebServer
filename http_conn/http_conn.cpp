#include "http_conn.h"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <utility>
#include <unistd.h>

HttpConnection::HttpConnection(const Config &config, int fd, Dispatcher *dispatcher, CloseCallback closeCallback)
    : m_dispatcher(dispatcher),
      m_channel(nullptr),
      m_closeCallback(std::move(closeCallback)),
      m_config(config),
      is_use_log(config.enableLogging)
{
    m_channel = new Channel(
        fd,
        FDEvent::ReadEvent,
        std::bind(&HttpConnection::processRead, this),
        std::bind(&HttpConnection::processWrite, this),
        std::bind(&HttpConnection::processClose, this));
    m_dispatcher->add(m_channel);
}

HttpConnection::~HttpConnection()
{
    delete m_channel;
}

void HttpConnection::processRead()
{
    const int result = m_config.triggerMode == TriggerMode::LevelTriggered ? LTRead() : ETRead();
    if (result < 0 || m_channel == nullptr)
    {
        return;
    }
}

// channel的销毁回调。先从反应堆中移除，再调用webServer的删除httpConn的回调
void HttpConnection::processClose()
{
    closeConnection();
}

int HttpConnection::LTRead()
{
    struct iovec vec[2];
    const int writeAble = m_readBuf.writeAbleSize();
    vec[0].iov_base = m_readBuf.writeStartPos();
    vec[0].iov_len = writeAble;

    char extraBuf[40960];
    vec[1].iov_base = extraBuf;
    vec[1].iov_len = sizeof(extraBuf);

    const int result = readv(m_channel->getFd(), vec, 2);
    if (result > 0)
    {
        if (result <= writeAble)
        {
            m_readBuf.updateWritePos(result);
        }
        else
        {
            m_readBuf.updateWritePos(writeAble);
            m_readBuf.appendData(extraBuf, result - writeAble);
        }
        return result;
    }

    if (result == 0)
    {
        LOG_INFO("关闭与{}的连接", m_channel->getFd());
        closeConnection();
        return -1;
    }

    if (errno == EINTR)
    {
        LOG_INFO("受到中断信号影响，可能出错");
        return 0;
    }
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
