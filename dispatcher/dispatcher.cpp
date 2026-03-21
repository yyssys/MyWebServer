#include "dispatcher.h"

Dispatcher::Dispatcher(const Config &config, bool enableTimer)
    : is_use_log(config.enableLogging), m_wakeupChannel(nullptr), m_wakeupFds{-1, -1},
      m_timer(config.enableLogging), m_timerfd(-1), m_ThreadId(std::this_thread::get_id()),
      m_config(config), timeout(false), m_enableTimer(enableTimer)
{
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, m_wakeupFds) != 0)
    {
        LOG_ERROR("socketpair failed.");
    }
    if (m_enableTimer)
    {
        m_timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
        if (m_timerfd == -1)
        {
            LOG_ERROR("timerfd_create failed.");
        }
    }
}

Dispatcher::~Dispatcher()
{
    m_channelMap.clear();
    close(m_wakeupFds[1]);
}

void Dispatcher::queueTask(std::function<void()> task)
{
    {
        std::lock_guard<std::mutex> lock(m_QueueMutex);
        m_TaskQueue.push_back(std::move(task));
    }
    notifyDispatcher();
}

void Dispatcher::registerTimeout(Channel *channel)
{
    if (channel == nullptr)
    {
        return;
    }
    if (!isInOwnerThread())
    {
        queueTask([this, channel]()
                  { this->registerTimeout(channel); });
        return;
    }
    m_timer.addOrUpdate(channel->getFd(), time(nullptr) + ConnectionIdleTimeout);
}

void Dispatcher::updateTimeout(Channel *channel)
{
    if (channel == nullptr)
    {
        return;
    }
    if (!isInOwnerThread())
    {
        queueTask([this, channel]()
                  { this->updateTimeout(channel); });
        return;
    }
    if (m_channelMap.find(channel->getFd()) == m_channelMap.end())
    {
        return;
    }
    m_timer.addOrUpdate(channel->getFd(), time(nullptr) + ConnectionIdleTimeout);
}

void Dispatcher::removeTimeout(Channel *channel)
{
    if (channel == nullptr)
    {
        return;
    }
    if (!isInOwnerThread())
    {
        queueTask([this, channel]()
                  { this->removeTimeout(channel); });
        return;
    }
    m_timer.remove(channel->getFd());
}

std::deque<std::function<void()>> Dispatcher::takeQueueElements()
{
    std::lock_guard<std::mutex> lock(m_QueueMutex);
    std::deque<std::function<void()>> tmpTaskQueue;
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
    std::deque<std::function<void()>> tasks = takeQueueElements();
    for (std::function<void()> &task : tasks)
    {
        task();
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

void Dispatcher::handleAlarm()
{
    uint64_t expirations = 0;
    while (true)
    {
        const ssize_t ret = read(m_timerfd, &expirations, sizeof(expirations));
        if (ret > 0)
        {
            // LOG_INFO("Timer expired {} times", expirations);
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

        LOG_ERROR("recv alarm failed.");
        break;
    }
    timeout = true;
}

void Dispatcher::processTimeout()
{
    const std::vector<int> expiredFds = m_timer.takeExpired(time(nullptr));
    timeout = false;
    for (int fd : expiredFds)
    {
        auto iter = m_channelMap.find(fd);
        if (iter == m_channelMap.end())
        {
            continue;
        }

        LOG_INFO("connection fd {} timeout", fd);
        iter->second->handleClose();
    }
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
    m_wakeupChannel.reset(new Channel(
        m_wakeupFds[0],
        FDEvent::ReadEvent,
        std::bind(&Dispatcher::handleWakeup, this),
        nullptr,
        nullptr));
    add(m_wakeupChannel.get());

    if (!m_enableTimer)
    {
        return;
    }

    m_alarmChannel.reset(new Channel(
        m_timerfd,
        FDEvent::ReadEvent,
        std::bind(&Dispatcher::handleAlarm, this),
        nullptr,
        nullptr));
    struct itimerspec new_value{};
    new_value.it_value.tv_sec = TimerInterval;
    new_value.it_interval.tv_sec = TimerInterval;
    if (timerfd_settime(m_timerfd, 0, &new_value, NULL) == -1)
    {
        LOG_ERROR("timerfd_settime");
    }
    add(m_alarmChannel.get());
}
