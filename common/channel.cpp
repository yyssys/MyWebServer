#include "channel.h"

Channel::Channel(int fd, FDEvent events, callback readFunc, callback writeFunc)
    : m_fd(fd), m_events(events), readCallback(readFunc), writeCallback(writeFunc)
{
}

void Channel::setListenWriteEvent(bool flag)
{
    if (flag)
    {
        m_events = m_events | FDEvent::WriteEvent;
    }
    else
    {
        // 取消对写事件的监听
        m_events = m_events & ~FDEvent::WriteEvent;
    }
}
