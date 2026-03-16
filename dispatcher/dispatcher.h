#pragma once
#include <deque>
#include <fcntl.h>
#include <mutex>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include "log/log.h"
#include "channel/channel.h"

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
    Dispatcher(bool useLog);
    // 添加
    virtual void add(Channel *Channel) {}
    // 删除
    virtual void remove(Channel *Channel) {}
    // 修改
    virtual void modify(Channel *Channel) {}
    // 事件监测
    virtual void dispatch(int timeout = 2) {} // 单位: s

    virtual ~Dispatcher();

protected:
    bool isInOwnerThread() const
    {
        return std::this_thread::get_id() == m_ThreadId;
    }
    // 向队列中添加元素
    void addElement(Channel *channel, Operation operation);

    // 取队列中的所有元素
    std::deque<ElementType> takeQueueElements();

    void notifyDispatcher()
    {
        const char Byte = 1;
        const int ret = send(m_wakeupFds[1], &Byte, 1, 0);
        if (ret < 0 && is_use_log)
        {
            LOG_ERROR("send wakeup signal failed.");
        }
    }
    // 处理任务队列
    void processTaskQueue();
    // 唤醒阻塞在poll,epoll,select的线程
    void handleWakeup();
    // 设置socket为非阻塞模式
    void setNonBlocking(int fd);
    // 初始化唤醒socket, 并将读端加入反应堆
    void initWakeupChannel();

    bool is_use_log;
    std::unordered_map<int, Channel *> m_channelMap; // Dispatcher 持有 Channel 所有权
    int m_wakeupFds[2];                              // // 唤醒套接字，0端读，1端写
    std::thread::id m_ThreadId;
    std::mutex m_QueueMutex;
    std::deque<ElementType> m_TaskQueue;
};
