#pragma once
#include <mutex>
#include <condition_variable>
template <class T>
class Block_Queue
{
public:
    Block_Queue(int max_size = 1000)
    {
        if (max_size <= 0)
        {
            exit(1);
        }
        m_queue_size = max_size;
        m_queue = new T[max_size];
        if (!m_queue)
        {
            exit(1);
        }
        m_front = -1;
        m_back = -1;
        m_curr_size = 0;
    }
    // 往队列里添加条目
    bool push(const T &item)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        // 如果队列已经满了，则通知消费者去消费，并返回false
        if (m_curr_size >= m_queue_size)
        {
            lock.unlock();
            m_cond.notify_one();
            return false;
        }
        m_back = (m_back + 1) % m_queue_size;
        m_queue[m_back] = item;
        m_curr_size++;
        lock.unlock();
        m_cond.notify_one();
        return true;
    }
    // 从对头取条目，利用参数传出
    bool pop(T &item)
    {
        // 如果队列为空，则阻塞等待，并设置阻塞时间100毫秒
        std::unique_lock<std::mutex> lock(m_mutex);
        if (!m_cond.wait_for(lock, std::chrono::milliseconds(100), [this]()
                             { return m_curr_size > 0; }))
        {
            return false;
        }
        m_front = (m_front + 1) % m_queue_size;
        item = m_queue[m_front];
        m_curr_size--;
        return true;
    }
    ~Block_Queue()
    {
        if (m_queue)
        {
            delete[] m_queue;
        }
    }

private:
    T *m_queue;               // 一个可以复用的动态数组，用于线程异步将日志内容写入文档中
    int m_queue_size;         // 创建动态数组的长度
    int m_front;              // 取元素标志位
    int m_back;               // 插入标志位
    int m_curr_size; // 当前队列中元素个数

    std::mutex m_mutex;             // 互斥访问队列以及互斥修改队列相关的属性
    std::condition_variable m_cond; // 队列为空时阻塞线程的条件变量
};
