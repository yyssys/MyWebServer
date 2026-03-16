#include "channel.h"

Channel::Channel(int fd, FDEvent events, Callback readFunc, Callback writeFunc)
    : m_fd(fd), m_events(events), m_readCallback(readFunc), m_writeCallback(writeFunc)
{
}

void Channel::setWriteEnabled(bool enabled)
{
    if (enabled)
    {
        m_events = m_events | FDEvent::WriteEvent;
    }
    else
    {
        // 取消对写事件的监听
        m_events = m_events & ~FDEvent::WriteEvent;
    }
}
