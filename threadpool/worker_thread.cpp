#include "worker_thread.h"

WorkerThread::WorkerThread(Config &config)
    : m_dispatcher(nullptr),
      m_isReady(false),
      isExit(false),
      is_use_log(config.enableLogging),
      m_config(config)

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
    switch (m_config.reactorType)
    {
    case ReactorType::Epoll:
        m_dispatcher = new EpollDispatcher(m_config);
        break;
    case ReactorType::Poll:
        m_dispatcher = new PollDispatcher(m_config);
        break;
    case ReactorType::Select:
        m_dispatcher = new SelectDispatcher(m_config);
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
