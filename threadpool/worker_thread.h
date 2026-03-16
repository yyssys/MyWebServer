#pragma once
#include <condition_variable>
#include <atomic>
#include <mutex>
#include "dispatcher/epoll_dispatcher.h"
#include "dispatcher/poll_dispatcher.h"
#include "dispatcher/select_dispatcher.h"
using namespace std;

// 定义子线程对应的结构体
class WorkerThread
{
public:
    WorkerThread(int model = 0, int triggerModel = 0, bool uselog = true);

    Dispatcher *getDispatcher() const
    {
        return m_dispatcher;
    }

    ~WorkerThread();

private:
    void worker();

    thread m_thread;           // 保存线程的实例
    mutex m_mutex;             // 互斥锁
    condition_variable m_cond; // 条件变量
    Dispatcher *m_dispatcher;  // 反应堆模型
    bool m_isReady;            // 反应堆是否初始化完成

    std::atomic<bool> isExit; // 子线程退出标志
    bool is_use_log;          // 日志开关
    int m_model;              // 采用什么反应堆模型
    int m_triggerModel;       // 反应堆模型触发模式，只有为epoll时才生效
};
