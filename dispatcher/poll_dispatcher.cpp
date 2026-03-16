#include "poll_dispatcher.h"

PollDispatcher::PollDispatcher(bool uselog) : Dispatcher(uselog), m_maxfd(0)
{
    for (int i = 0; i < MaxNode; ++i)
    {
        m_pollfd[i].fd = -1;
        m_pollfd[i].events = 0;
        m_pollfd[i].revents = 0;
    }

    initWakeupChannel();
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

void PollDispatcher::remove(Channel *channel)
{
    if (channel == nullptr || m_channelMap.find(channel->getFd()) == m_channelMap.end())
    {
        LOG_ERROR("poll remove {} failed", channel->getFd());
        return;
    }
    for (int i = 0; i < MaxNode; ++i)
    {
        if (m_pollfd[i].fd == channel->getFd())
        {
            m_pollfd[i].events = 0;
            m_pollfd[i].revents = 0;
            m_pollfd[i].fd = -1;
            break;
        }
    }
    m_channelMap.erase(channel->getFd());
    delete channel;
}

void PollDispatcher::modify(Channel *channel)
{
    if (channel == nullptr || m_channelMap.find(channel->getFd()) == m_channelMap.end())
    {
        LOG_ERROR("poll modify {} failed", channel->getFd());
        return;
    }
    int events = 0;
    if ((channel->getEvents() & FDEvent::ReadEvent) != FDEvent::None)
    {
        events |= POLLIN;
    }
    if ((channel->getEvents() & FDEvent::WriteEvent) != FDEvent::None)
    {
        events |= POLLOUT;
    }
    for (int i = 0; i < MaxNode; ++i)
    {
        if (m_pollfd[i].fd == channel->getFd())
        {
            m_pollfd[i].events |= events;
            break;
        }
    }
}

void PollDispatcher::dispatch(int timeout)
{
    int count = poll(m_pollfd, m_maxfd + 1, timeout * 1000);
    if (count == -1)
    {
        LOG_ERROR("poll error");
        exit(0);
    }
    for (int i = 0; i <= m_maxfd; ++i)
    {
        if (m_pollfd[i].fd == -1)
        {
            continue;
        }
        auto iter = m_channelMap.find(m_pollfd[i].fd);
        if (iter == m_channelMap.end())
        {
            LOG_ERROR("poll 激活的fd不在map中");
            continue;
        }
        Channel *channel = iter->second;
        const int readyEvents = m_pollfd[i].revents;
        if (readyEvents & (POLLHUP | POLLERR))
        {
            // 对端关闭，移除
            remove(channel);
        }
        else if ((readyEvents & (POLLIN | POLLRDHUP)) != 0)
        {
            channel->handleRead();
        }
        if ((readyEvents & POLLOUT) != 0)
        {
            channel->handleWrite();
        }
    }
}
