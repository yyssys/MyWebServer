#include "dispatcher.h"

Dispatcher::Dispatcher(const Config &config) : is_use_log(config.enableLogging), m_wakeupFds{-1, -1},
                                               m_ThreadId(std::this_thread::get_id()), m_config(config)
{
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, m_wakeupFds) != 0)
    {
        LOG_ERROR("socketpair failed.");
    }
}

Dispatcher::~Dispatcher()
{
    for (auto &item : m_channelMap)
    {
        delete item.second;
    }
    m_channelMap.clear();
    close(m_wakeupFds[1]);
}

void Dispatcher::addElement(Channel *channel, Operation operation)
{
    {
        std::lock_guard<std::mutex> lock(m_QueueMutex);
        m_TaskQueue.push_back({channel, operation});
    }
    notifyDispatcher();
}

std::deque<ElementType> Dispatcher::takeQueueElements()
{
    std::lock_guard<std::mutex> lock(m_QueueMutex);
    std::deque<ElementType> tmpTaskQueue;
    tmpTaskQueue.swap(m_TaskQueue);
    return tmpTaskQueue;
}

void Dispatcher::notifyDispatcher()
{
    const char Byte = 1;
    const int ret = send(m_wakeupFds[1], &Byte, 1, 0);
    if (ret < 0 && is_use_log)
    {
        LOG_ERROR("send wakeup signal failed.");
    }
}

void Dispatcher::processTaskQueue()
{
    std::deque<ElementType> elementtype = takeQueueElements();
    for (const ElementType &et : elementtype)
    {
        switch (et.operation)
        {
        case Operation::Add:
            add(et.channel);
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

void Dispatcher::handleWakeup()
{
    char buffer[8];
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

void Dispatcher::setNonBlocking(int fd)
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

void Dispatcher::initWakeupChannel()
{
    setNonBlocking(m_wakeupFds[0]);
    add(new Channel(
        m_wakeupFds[0],
        FDEvent::ReadEvent,
        std::bind(&Dispatcher::handleWakeup, this),
        nullptr));
}
