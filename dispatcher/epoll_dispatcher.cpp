#include <cerrno>
#include <cstring>
#include <fcntl.h>

#include "epoll_dispatcher.h"

EpollDispatcher::EpollDispatcher(int triggerMode)
    : Dispatcher(), m_epollFd(-1), m_triggerMode(triggerMode), m_wakeupChannel(nullptr)
{
    m_epollFd = epoll_create(10);
    if (m_epollFd == -1)
    {
        LOG_ERROR("epoll_create failed.");
    }
    // 读端设置为非阻塞的
    setNonBlocking(m_wakeupFds[0]);

    m_wakeupChannel = new Channel(
        m_wakeupFds[0],
        FDEvent::ReadEvent,
        std::bind(&EpollDispatcher::handleWakeup, this),
        nullptr);
    addInLoop(m_wakeupChannel);
}

EpollDispatcher::~EpollDispatcher()
{
    if (m_wakeupChannel)
        delete m_wakeupChannel;
    close(m_wakeupFds[0]);
    close(m_wakeupFds[1]);
    close(m_epollFd);
}

void EpollDispatcher::add(Channel *channel)
{
    // 当前反应堆线程添加的话，直接处理就行，不需要先添加到队列中
    if (isInOwnerThread())
    {
        addInLoop(channel);
        return;
    }
    // 主线程操作的话，需要先添加到阻塞队列中
    addElement(channel, Operation::Add);
}

void EpollDispatcher::addInLoop(Channel *channel)
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
    delete channel;
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
            continue;
        }

        Channel *channel = iter->second;
        const int readyEvents = m_epollEvents[i].events;
        if (readyEvents & (EPOLLHUP | EPOLLERR))
        {
            // 对端关闭，移除
            remove(channel);
            
        }
        else if(fd == m_wakeupFds[0] && (readyEvents & EPOLLIN))
        {
            // 主线程发信号，有新的fd需要添加到epoll模型中。处理任务队列
            processTaskQueue();
        }
        else if ((readyEvents & (EPOLLIN | EPOLLRDHUP)) != 0)
        {
            channel->handleRead();
        }
        if ((readyEvents & EPOLLOUT) != 0)
        {
            channel->handleWrite();
        }
    }
}

void EpollDispatcher::handleWakeup()
{
    char buffer[8];
    // 将数据读完
    while (true)
    {
        const int ret = recv(m_wakeupFds[0], buffer, sizeof(buffer), 0);
        if (ret > 0)
        {
            continue;
        }
        if (ret == 0)
        {
            break;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            break;
        }

        LOG_ERROR("recv wakeup signal failed.");
        break;
    }

    processTaskQueue();
}

void EpollDispatcher::processTaskQueue()
{
    std::deque<ElementType> elementtype = takeQueueElements();
    for (const ElementType &et : elementtype)
    {
        switch (et.operation)
        {
        case Operation::Add:
            addInLoop(et.channel);
            break;
        case Operation::Modify:
            modify(et.channel);
            break;
        case Operation::Remove:
            remove(et.channel);
            break;
        default:
            break;
        }
    }
}

void EpollDispatcher::setNonBlocking(int fd)
{
    const int oldFlags = fcntl(fd, F_GETFL);
    if (oldFlags < 0)
    {
        LOG_ERROR("fcntl get flags failed for fd {}", fd);
        return;
    }

    if (fcntl(fd, F_SETFL, O_NONBLOCK | oldFlags) < 0)
    {
        LOG_ERROR("fcntl set nonblock failed for fd {}", fd);
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

    if (m_triggerMode != 0)
    {
        events |= EPOLLET;
        setNonBlocking(channel->getFd());
    }

    ev.events = events;
    return epoll_ctl(m_epollFd, op, channel->getFd(), &ev);
}
