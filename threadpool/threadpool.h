#pragma once
#include <thread>
#include <vector>
#include "dispatcher/dispatcher.h"
#include "worker_thread.h"
class ThreadPool
{
public:
    ThreadPool(Dispatcher *mainDispatcher, int num = std::thread::hardware_concurrency());
    ~ThreadPool();

    Dispatcher *getDispatcher();
    // 禁止拷贝
    ThreadPool(const ThreadPool &) = delete;
    ThreadPool &operator=(const ThreadPool &) = delete;

private:
    // 主线程的反应堆
    Dispatcher *m_mainDispatcher;

    bool m_isStart;
    int m_threadNum;
    std::vector<WorkerThread *> m_workerThreads;
    int m_index;
};
