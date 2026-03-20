#pragma once
#include <deque>
#include <functional>
#include <fcntl.h>
#include <mutex>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <sys/timerfd.h>
#include "log/log.h"
#include "channel/channel.h"
#include "config/config.h"
#include "timer/timer.h"

class Dispatcher
{
public:
    Dispatcher(const Config &config, bool enableTimer);
    // 添加
    virtual void add(Channel *channel) {}
    // 删除
    virtual void remove(Channel *channel) {}
    // 修改
    virtual void modify(Channel *channel) {}
    // 事件监测
    virtual void dispatch(int timeout = 2) {} // 单位: s
    void queueTask(std::function<void()> task);
    void registerTimeout(Channel *channel);
    void updateTimeout(Channel *channel);
    void removeTimeout(Channel *channel);

    virtual ~Dispatcher();

protected:
    bool isInOwnerThread() const
    {
        return std::this_thread::get_id() == m_ThreadId;
    }
    // 取队列中的所有元素
    std::deque<std::function<void()>> takeQueueElements();
    // 唤醒阻塞在poll,epoll,select的线程
    void notifyDispatcher();
    // 处理任务队列
    void processTaskQueue();
    // 唤醒后读空数据并处理任务队列
    void handleWakeup();
    // 定时器处理函数
    void handleAlarm();
    void processTimeout();
    // 设置socket为非阻塞模式
    void setNonBlocking(int fd);
    // 初始化唤醒socket, 并将读端加入反应堆
    void initWakeupChannel();

    bool is_use_log;
    std::unordered_map<int, Channel *> m_channelMap;
    std::unique_ptr<Channel> m_wakeupChannel;
    std::unique_ptr<Channel> m_alarmChannel;
    int m_wakeupFds[2]; // 唤醒套接字，0端读，1端写
    int m_timerfd;      // 定时器套接字
    std::thread::id m_ThreadId;
    std::mutex m_QueueMutex;
    std::deque<std::function<void()>> m_TaskQueue;
    Timer m_timer;
    Config m_config;
    bool timeout;
    bool m_enableTimer;
    static constexpr time_t TimerInterval = 5;
    static constexpr time_t ConnectionIdleTimeout = 15;
};
