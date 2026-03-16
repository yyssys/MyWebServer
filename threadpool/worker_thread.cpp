#include "worker_thread.h"

WorkerThread::WorkerThread(int model = 0, int triggerModel, bool uselog = true)
    : m_model(model), is_use_log(uselog), m_triggerModel(triggerModel), isExit(false), m_dispatcher(nullptr)
{
    // 创建子线程
    m_thread = thread(&WorkerThread::worker, this);
}

WorkerThread::~WorkerThread()
{
    isExit.store(true, std::memory_order::memory_order_release);
    if (m_thread.joinable())
    {
        m_thread.join();
    }
    if (m_dispatcher)
    {
        delete m_dispatcher;
    }
}

void WorkerThread::worker()
{
    switch (m_model)
    {
    case 0:
        m_dispatcher = new EpollDispatcher(is_use_log, m_triggerModel);
        break;
    case 1:
        m_dispatcher = new PollDispatcher(is_use_log);
        break;
    case 2:
        m_dispatcher = new SelectDispatcher(is_use_log);
        break;
    default:
        LOG_ERROR("反应堆模型选择错误");
        exit(0);
    }
    while (!isExit.load(std::memory_order::memory_order_acquire))
    {
        m_dispatcher->dispatch();
    }
}
