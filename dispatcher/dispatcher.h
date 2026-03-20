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
#include "config/config.h"

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
    Dispatcher(const Config &config);
    // 添加
    virtual void add(Channel *channel) {}
    // 删除
    virtual void remove(Channel *channel) {}
    // 修改
    virtual void modify(Channel *channel) {}
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
    // 唤醒阻塞在poll,epoll,select的线程
    void notifyDispatcher();
    // 处理任务队列
    void processTaskQueue();
    // 唤醒后读空数据并处理任务队列
    void handleWakeup();
    // 设置socket为非阻塞模式
    void setNonBlocking(int fd);
    // 初始化唤醒socket, 并将读端加入反应堆
    void initWakeupChannel();

    bool is_use_log;
    std::unordered_map<int, Channel *> m_channelMap;
    std::unique_ptr<Channel> m_wakeupChannel;
    int m_wakeupFds[2]; // 唤醒套接字，0端读，1端写
    std::thread::id m_ThreadId;
    std::mutex m_QueueMutex;
    std::deque<ElementType> m_TaskQueue;
    Config m_config;
};
