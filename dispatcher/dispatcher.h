#pragma once
#include <deque>
#include <mutex>
#include <unistd.h>
#include <sys/socket.h>
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
    Dispatcher(bool useLog = true) : is_use_log(useLog), m_ThreadId(std::this_thread::get_id())
    {
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, m_wakeupFds) != 0)
        {
            LOG_ERROR("socketpair failed.");
        }
    }
    // 添加
    virtual void add(Channel *Channel) {};
    // 删除
    virtual void remove(Channel *Channel) {};
    // 修改
    virtual void modify(Channel *Channel) {};
    // 事件监测
    virtual void dispatch(int timeout = 2) {}; // 单位: s

    virtual ~Dispatcher() {}

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

    bool is_use_log;
    std::unordered_map<int, Channel *> m_channelMap; // 存储fd与channel的关系
    // 0读，1写
    int m_wakeupFds[2];
    std::thread::id m_ThreadId;
    std::mutex m_QueueMutex;
    std::deque<ElementType> m_TaskQueue;

private:
};
