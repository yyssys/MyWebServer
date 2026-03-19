#include <sstream>
#include <iostream>
#include "log.h"

void Log::init(const Config &config, std::string file)
{
    // 使用异步写
    if (config.logWriteMode == LogWriteMode::Async)
    {
        m_isAyscWrite = true;
        m_block_queue = new Block_Queue<std::string>(2000);
        m_async_thread = std::thread(&Log::aysc_write, this);
    }

    // 获取时间戳
    std::time_t time_now = time(nullptr);
    // 将时间戳转换为本地时间
    std::tm tm{};
    localtime_r(&time_now, &tm);
    m_today = tm.tm_mday;
    // 只需年月日即可
    std::string time_str = getTimeString(tm).substr(0, 10);

    // 解析文件路径和文件名
    std::string log_full_name;
    size_t point;
    if ((point = file.rfind('/')) != std::string::npos)
    {
        m_file_path = file.substr(0, point + 1);
        m_file_name = file.substr(point + 1);
        log_full_name = m_file_path + time_str + m_file_name;
    }
    else
    {
        log_full_name = time_str + file;
    }

    m_file.open(log_full_name, std::ios::app);
    if (!m_file)
    {
        exit(1);
    }
}

Log::~Log()
{
    m_exit_flag.store(true, std::memory_order_release);
    if (m_isAyscWrite && m_async_thread.joinable())
    {
        m_async_thread.join();
    }
    if (m_file.is_open())
    {
        m_file.close();
    }
    if (m_block_queue)
    {
        delete m_block_queue;
    }
}

void Log::aysc_write()
{
    std::string log_msg;
    // 从阻塞队列中取出一条日志信息写入文件
    while (!m_exit_flag.load(std::memory_order_acquire))
    {
        if (m_block_queue->pop(log_msg))
        {
            // 这里加锁是为了防止其他线程因达到日期阈值而关闭文件
            // 加锁后就算其他线程关闭了文件，也会再打开一个新的，这里写入的就是新文件了
            std::lock_guard<std::mutex> lock(m_mutex);
            m_file << log_msg << "\n";
            m_file.flush();
        }
        else
        {
            continue;
        }
    }
    // 即使退出了循环，队列里可能还有最后几条残留日志，必须把它们写完！
    // 因为 m_exit_flag=true 时，可能刚好有一条日志在队列里，但还没被 pop 出来
    while (m_block_queue->pop(log_msg))
    {
        m_file << log_msg << "\n";
        m_file.flush();
    }
}

std::string Log::getTimeString(std::tm &tm)
{
    // std::put_time 的返回值是输出流操作器，不能直接赋值给变量，只能插入到输出流中使用；
    // 若要获取格式化后的字符串，需通过 std::ostringstream 中转（先插入操作器，再调用 str() 方法）
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return std::string(oss.str());
}
