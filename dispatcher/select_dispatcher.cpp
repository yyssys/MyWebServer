#include "select_dispatcher.h"

void SelectDispatcher::add(Channel *channel)
{
    // 当前反应堆线程添加的话，直接处理就行，不需要先添加到队列中
    if (isInOwnerThread())
    {
        if (channel == nullptr || m_channelMap.find(channel->getFd()) != m_channelMap.end())
        {
            LOG_ERROR("select add error");
            return;
        }
        // select可检测的fd值最大为1023
        if (channel->getFd() > 1023)
        {
            LOG_ERROR("select fd值越界");
            return;
        }
        setFdSet(channel);
        m_channelMap[channel->getFd()] = channel;
        return;
    }
    // 主线程操作的话，需要先添加到阻塞队列中
    addElement(channel, Operation::Add);
}

void SelectDispatcher::remove(Channel *channel)
{
    if (channel == nullptr || m_channelMap.find(channel->getFd()) == m_channelMap.end())
    {
        LOG_ERROR("select remove error");
        return;
    }
    FD_CLR(channel->getFd(), &m_readSet);
    FD_CLR(channel->getFd(), &m_writeSet);
    m_channelMap.erase(channel->getFd());
    delete channel;
}

void SelectDispatcher::modify(Channel *channel)
{
    if (channel == nullptr || m_channelMap.find(channel->getFd()) == m_channelMap.end())
    {
        LOG_ERROR("select modify error");
        return;
    }
    FD_CLR(channel->getFd(), &m_readSet);
    FD_CLR(channel->getFd(), &m_writeSet);
    setFdSet(channel);
}

void SelectDispatcher::dispatch(int timeout)
{
    struct timeval val;
    val.tv_sec = timeout;
    val.tv_usec = 0;
    fd_set rdtmp = m_readSet;
    fd_set wrtmp = m_writeSet;
    int count = select(1024, &rdtmp, &wrtmp, NULL, &val);
    if (count == -1)
    {
        perror("select");
        exit(0);
    }
    for (int i = 0; i < 1024; ++i)
    {
        if (!FD_ISSET(i, &rdtmp) && !FD_ISSET(i, &wrtmp))
        {
            continue;
        }
        auto iter = m_channelMap.find(i);
        if (iter == m_channelMap.end())
        {
            LOG_ERROR("select 激活的fd不在map中");
            continue;
        }
        Channel *channel = iter->second;
        if (FD_ISSET(i, &rdtmp))
        {
            channel->handleRead();

            iter = m_channelMap.find(i);
            if (iter == m_channelMap.end())
            {
                continue;
            }
            channel = iter->second;
        }

        if (FD_ISSET(i, &wrtmp))
        {
            channel->handleWrite();
        }
    }
}

void SelectDispatcher::setFdSet(Channel *channel)
{
    if ((channel->getEvents() & FDEvent::ReadEvent) != FDEvent::None)
    {
        FD_SET(channel->getFd(), &m_readSet);
    }
    if ((channel->getEvents() & FDEvent::WriteEvent) != FDEvent::None)
    {
        FD_SET(channel->getFd(), &m_writeSet);
    }
}
