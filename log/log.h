#pragma once
#include <string>
#include <mutex>
#include <fstream>
#include <atomic>
#include <thread>
#include <ctime>
#include <iomanip>
#include <fmt/core.h>
#include <iostream>

#include "block_queue.h"

class Log
{
public:
    static Log *getInstance()
    {
        static Log log;
        return &log;
    }
    // 单例模式禁用拷贝构造和拷贝赋值运算符。有析构，无移动
    Log(const Log &) = delete;
    Log &operator=(const Log &) = delete;
    // 如果定义了队列长度，则使用异步写， 默认使用异步写
    void init(std::string file = "./server.log", int queue_size = 10000);
    // 写入日志
    template <class... Args>
    void write_log(int level, const std::string &file, int line, const std::string &format, Args... args);

    template <class... Args>
    void print(const std::string &file, int line, const std::string &format, Args... args);
    ~Log();

private:
    // 回调函数，创建线程执行
    void aysc_write();
    std::string getTimeString(std::tm &tm);

private:
    Log() : m_block_queue(nullptr), m_today(0),
            m_isAyscWrite(false), m_exit_flag(false) {};

    Block_Queue<std::string> *m_block_queue; // 异步日志队列
    std::string m_file_path;                 // 日志文件路径
    std::string m_file_name;                 // 日志文件名
    int m_today;                             // 记录当前日志文件生成的日期
    bool m_isAyscWrite;                      // 是否异步写日志
    std::ofstream m_file;                    // 日志文件流
    std::atomic<bool> m_exit_flag;           // 异步线程退出标志
    std::thread m_async_thread;              // 异步写日志线程句柄
    std::mutex m_mutex;
};

template <class... Args>
inline void Log::write_log(int level, const std::string &file, int line, const std::string &format, Args... args)
{
    // 获取时间戳
    std::time_t time_now = time(nullptr);
    // 将时间戳转换为本地时间
    std::tm tm{};
    localtime_r(&time_now, &tm);

    std::string time_str = getTimeString(tm);

    // 日志文件按天划分
    if (m_today != tm.tm_mday)
    {
        m_today = tm.tm_mday;
        std::unique_lock<std::mutex> lock(m_mutex);
        m_file.close();
        std::string new_log_name = m_file_path + time_str + m_file_name;
        m_file.open(new_log_name, std::ios::app);
    }
    std::string log_msg = time_str + " " + file + ":" + std::to_string(line) + " ";
    switch (level)
    {
    case 0:
        log_msg += "[info]: ";
        break;
    case 1:
        log_msg += "[error]: ";
        break;
    }

    // 解析格式化字符串
    log_msg += fmt::format(format, std::forward<Args>(args)...);
    if (m_isAyscWrite) // 异步写
    {
        // 队列满了，10微秒后再试
        while (!m_block_queue->push(log_msg))
        {
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
    }
    else // 同步写，需要加锁，防止多线程同时写造成写缓冲区数据混乱
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_file << log_msg << "\n";
    }
}

template <class... Args>
inline void Log::print(const std::string &file, int line, const std::string &format, Args... args)
{
    std::cout << file << ":" << line << ": " << fmt::format(format, args...) << std::endl;
}

#define LOG_INFO(fmt, ...)                                                            \
    do                                                                                   \
    {                                                                                    \
        if (is_use_log)                                                                  \
        {                                                                                \
            Log::getInstance()->write_log(0, __FILE__, __LINE__, fmt, ##__VA_ARGS__); \
        }                                                                                \
    } while (0)

#define LOG_ERROR(fmt, ...)                                                           \
    do                                                                                   \
    {                                                                                    \
        if (is_use_log)                                                                  \
        {                                                                                \
            Log::getInstance()->write_log(1, __FILE__, __LINE__, fmt, ##__VA_ARGS__); \
        }                                                                                \
    } while (0)

#define LOG_PRINT(fmt, ...)                                                   \
    do                                                                        \
    {                                                                         \
        Log::getInstance()->print(__FILE__, __LINE__, fmt, ##__VA_ARGS__); \
    } while (0)
