#include "epoll_dispatcher.h"

EpollDispatcher::EpollDispatcher(const Config &config)
    : Dispatcher(config), m_epollFd(-1)
{
    m_epollFd = epoll_create(10);
    if (m_epollFd == -1)
    {
        LOG_ERROR("epoll_create failed.");
    }
    initWakeupChannel();
}

EpollDispatcher::~EpollDispatcher()
{
    close(m_epollFd);
}

void EpollDispatcher::add(Channel *channel)
{
    // 当前反应堆线程添加的话，直接处理就行，不需要先添加到队列中
    if (isInOwnerThread())
    {
        if (channel == nullptr || m_channelMap.find(channel->getFd()) != m_channelMap.end())
        {
            return;
        }

        if (updateEpoll(channel, EPOLL_CTL_ADD) != 0)
        {
            LOG_ERROR("epoll_ctl add {} failed", channel->getFd());
            return;
        }
        m_channelMap[channel->getFd()] = channel;
        return;
    }
    // 主线程操作的话，需要先添加到阻塞队列中
    addElement(channel, Operation::Add);
}

void EpollDispatcher::remove(Channel *channel)
{
    if (channel == nullptr || m_channelMap.find(channel->getFd()) == m_channelMap.end())
    {
        return;
    }

    if (updateEpoll(channel, EPOLL_CTL_DEL) != 0)
    {
        LOG_ERROR("epoll_ctl del {} failed", channel->getFd());
    }
    m_channelMap.erase(channel->getFd());
}

void EpollDispatcher::modify(Channel *channel)
{
    if (channel == nullptr || m_channelMap.find(channel->getFd()) == m_channelMap.end())
    {
        return;
    }

    if (updateEpoll(channel, EPOLL_CTL_MOD) != 0)
    {
        LOG_ERROR("epoll_ctl modify {} failed", channel->getFd());
    }
}

void EpollDispatcher::dispatch(int timeout)
{
    const int count = epoll_wait(m_epollFd, m_epollEvents, 1024, timeout * 1000);
    if (count < 0)
    {
        if (errno != EINTR)
        {
            LOG_ERROR("epoll_wait failed.");
        }
        return;
    }

    for (int i = 0; i < count; ++i)
    {
        const int fd = m_epollEvents[i].data.fd;
        auto iter = m_channelMap.find(fd);
        if (iter == m_channelMap.end())
        {
            LOG_ERROR("epoll 激活的fd不在map中");
            continue;
        }

        Channel *channel = iter->second;
        const int readyEvents = m_epollEvents[i].events;
        if (readyEvents & (EPOLLHUP | EPOLLERR))
        {
            channel->handleClose();
            continue;
        }
        if ((readyEvents & (EPOLLIN | EPOLLRDHUP)) != 0)
        {
            channel->handleRead();

            // 读回调可能已经移除了channel，后续不能再访问悬空指针
            iter = m_channelMap.find(fd);
            if (iter == m_channelMap.end())
            {
                continue;
            }
            channel = iter->second;
        }
        if ((readyEvents & EPOLLOUT) != 0)
        {
            channel->handleWrite();
        }
    }
}

int EpollDispatcher::updateEpoll(Channel *channel, int op)
{
    struct epoll_event ev{};
    ev.data.fd = channel->getFd();

    int events = 0;
    if ((channel->getEvents() & FDEvent::ReadEvent) != FDEvent::None)
    {
        events |= EPOLLIN | EPOLLRDHUP;
    }

    if ((channel->getEvents() & FDEvent::WriteEvent) != FDEvent::None)
    {
        events |= EPOLLOUT | EPOLLRDHUP;
    }

    if (m_config.triggerMode == TriggerMode::ET)
    {
        events |= EPOLLET;
    }
    setNonBlocking(channel->getFd());
    ev.events = events;
    return epoll_ctl(m_epollFd, op, channel->getFd(), &ev);
}
