#include "poll_dispatcher.h"

PollDispatcher::PollDispatcher(bool uselog) : Dispatcher(uselog), m_maxfd(0)
{
    for (int i = 0; i < MaxNode; ++i)
    {
        m_pollfd[i].fd = -1;
        m_pollfd[i].events = 0;
        m_pollfd[i].revents = 0;
    }
}

void PollDispatcher::add(Channel *channel)
{
    // 当前反应堆线程添加的话，直接处理就行，不需要先添加到队列中
    if (isInOwnerThread())
    {
        int events = POLLRDHUP;
        if ((channel->getEvents() & FDEvent::ReadEvent) != FDEvent::None)
        {
            events |= POLLIN;
        }
        if ((channel->getEvents() & FDEvent::WriteEvent) != FDEvent::None)
        {
            events |= POLLOUT;
        }
        int i = 0;
        for (; i < MaxNode; ++i)
        {
            if (m_pollfd[i].fd == -1)
            {
                m_pollfd[i].fd = channel->getFd();
                m_pollfd[i].events = events;
                m_maxfd = i > m_maxfd ? i : m_maxfd;
                break;
            }
        }
        if (i >= MaxNode)
        {
            LOG_ERROR("连接数量过多！");
        }
        return;
    }
    // 主线程操作的话，需要先添加到阻塞队列中
    addElement(channel, Operation::Add);
}
