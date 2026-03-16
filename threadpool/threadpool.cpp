#include "threadpool.h"

ThreadPool::ThreadPool(Dispatcher *mainDispatcher, int num)
    : m_mainDispatcher(mainDispatcher), m_isStart(false), m_threadNum(num), m_index(0)
{
    for (int i = 0; i < m_threadNum; ++i)
    {
        WorkerThread *subThread = new WorkerThread();
        m_workerThreads.push_back(subThread);
    }
}

ThreadPool::~ThreadPool()
{
    for (auto &tmp : m_workerThreads)
    {
        delete tmp;
    }
}

Dispatcher *ThreadPool::getDispatcher()
{
    Dispatcher *dispatcher = m_mainDispatcher;
    if (m_threadNum > 0)
    {
        dispatcher = m_workerThreads[m_index]->getDispatcher();
        m_index = (m_index + 1) % m_threadNum;
    }
    return dispatcher;
}
