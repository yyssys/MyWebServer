#pragma once
#include <deque>
#include <fcntl.h>
#include <mutex>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include "log/log.h"
#include "common/channel.h"

enum class Operation
{
    Add = 0,
    Modify = 1,
    Remove = 2
};

struct ElementType
{
    Channel *channel;
    Operation operation;
};

class Dispatcher
{
public:
    Dispatcher(bool useLog)
        : is_use_log(useLog),
          m_wakeupFds{-1, -1},
          m_ThreadId(std::this_thread::get_id()),
          m_wakeupChannel(nullptr)
    {
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, m_wakeupFds) != 0)
        {
            LOG_ERROR("socketpair failed.");
        }
    }
    // 添加
    virtual void add(Channel *Channel) {}
    // 删除
    virtual void remove(Channel *Channel) {}
    // 修改
    virtual void modify(Channel *Channel) {}
    // 事件监测
    virtual void dispatch(int timeout = 2) {} // 单位: s

    virtual ~Dispatcher()
    {
        delete m_wakeupChannel;
        close(m_wakeupFds[1]);
    }

protected:
    bool isInOwnerThread() const
    {
        return std::this_thread::get_id() == m_ThreadId;
    }
    // 向队列中添加元素
    void addElement(Channel *channel, Operation operation)
    {
        {
            std::lock_guard<std::mutex> lock(m_QueueMutex);
            m_TaskQueue.push_back({channel, operation});
        }
        notifyDispatcher();
    }
    // 取队列中的所有元素
    std::deque<ElementType> takeQueueElements()
    {
        std::lock_guard<std::mutex> lock(m_QueueMutex);
        std::deque<ElementType> tmpTaskQueue;
        tmpTaskQueue.swap(m_TaskQueue);
        return tmpTaskQueue;
    }

    void notifyDispatcher()
    {
        const char Byte = 1;
        const int ret = send(m_wakeupFds[1], &Byte, 1, 0);
        if (ret < 0 && is_use_log)
        {
            LOG_ERROR("send wakeup signal failed.");
        }
    }

    void processTaskQueue()
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

    void handleWakeup()
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

    void setNonBlocking(int fd)
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

    void initWakeupChannel()
    {
        setNonBlocking(m_wakeupFds[0]);
        m_wakeupChannel = new Channel(
            m_wakeupFds[0],
            FDEvent::ReadEvent,
            std::bind(&Dispatcher::handleWakeup, this),
            nullptr);
        add(m_wakeupChannel);
    }

    bool is_use_log;
    std::unordered_map<int, Channel *> m_channelMap; // 存储fd与channel的关系
    // 0读，1写
    int m_wakeupFds[2];
    std::thread::id m_ThreadId;
    std::mutex m_QueueMutex;
    std::deque<ElementType> m_TaskQueue;
    Channel *m_wakeupChannel;
};
