#include "threadpool.h"

ThreadPool::ThreadPool(Dispatcher *mainDispatcher, const Config &config)
    : m_mainDispatcher(mainDispatcher),
      m_isStart(false),
      is_use_log(config.enableLogging),
      m_index(0),
      m_config(config)
{
    for (int i = 0; i < config.workerThreadCount; ++i)
    {
        WorkerThread *subThread = new WorkerThread(config);
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
    if (m_config.workerThreadCount > 0)
    {
        dispatcher = m_workerThreads[m_index]->getDispatcher();
        m_index = (m_index + 1) % m_config.workerThreadCount;
    }
    return dispatcher;
}
