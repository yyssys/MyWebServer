#include "worker_thread.h"

WorkerThread::WorkerThread(int model, int triggerModel, bool uselog)
    : m_dispatcher(nullptr),
      m_isReady(false),
      isExit(false),
      is_use_log(uselog),
      m_model(model),
      m_triggerModel(triggerModel)
{
    // 创建子线程
    m_thread = thread(&WorkerThread::worker, this);
    // 主线程必须等待子线程实例化m_dispatcher指针后再返回，避免getDispatcher()返回空指针
    unique_lock<mutex> lock(m_mutex);
    m_cond.wait(lock, [this]
                { return m_isReady; });
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
    Dispatcher *dispatcher = nullptr;
    switch (m_model)
    {
    case 0:
        dispatcher = new EpollDispatcher(is_use_log, m_triggerModel);
        break;
    case 1:
        dispatcher = new PollDispatcher(is_use_log);
        break;
    case 2:
        dispatcher = new SelectDispatcher(is_use_log);
        break;
    default:
        LOG_ERROR("反应堆模型选择错误");
        exit(0);
    }
    {
        lock_guard<mutex> lock(m_mutex);
        m_dispatcher = dispatcher;
        m_isReady = true;
    }
    m_cond.notify_one();
    while (!isExit.load(std::memory_order::memory_order_acquire))
    {
        m_dispatcher->dispatch();
    }
}
