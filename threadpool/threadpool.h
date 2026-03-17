#pragma once
#include <thread>
#include <vector>
#include "dispatcher/dispatcher.h"
#include "worker_thread.h"
class ThreadPool
{
public:
    ThreadPool(Dispatcher *mainDispatcher, const Config &config);
    ~ThreadPool();

    Dispatcher *getDispatcher();
    // 禁止拷贝
    ThreadPool(const ThreadPool &) = delete;
    ThreadPool &operator=(const ThreadPool &) = delete;

private:
    // 主线程的反应堆
    Dispatcher *m_mainDispatcher;

    bool is_use_log;
    Config m_config;
    bool m_isStart;
    std::vector<WorkerThread *> m_workerThreads;
    int m_index;
};
