#pragma once
#include <functional>
// 定义文件描述符的读写事件
enum class FDEvent : char
{
    None = 0,
    ReadEvent = 1 << 0,
    WriteEvent = 1 << 1
};
inline FDEvent operator|(FDEvent a, FDEvent b)
{
    return static_cast<FDEvent>(static_cast<char>(a) | static_cast<char>(b));
}

inline FDEvent operator&(FDEvent a, FDEvent b)
{
    return static_cast<FDEvent>(static_cast<char>(a) & static_cast<char>(b));
}

inline FDEvent operator~(FDEvent a)
{
    return static_cast<FDEvent>(~static_cast<char>(a));
}
class Channel
{
public:
    using Callback = std::function<void()>;

    Channel(int fd, FDEvent events, Callback readFunc, Callback writeFunc);

    // 是否监听写事件
    void setWriteEnabled(bool enabled);

    // 获取 fd
    int getFd() const
    {
        return m_fd;
    }

    FDEvent getEvents() const
    {
        return m_events;
    }

    void handleRead() const
    {
        if (m_readCallback)
        {
            m_readCallback();
        }
    }

    void handleWrite() const
    {
        if (m_writeCallback)
        {
            m_writeCallback();
        }
    }

    // 回调函数
    Callback m_readCallback;
    Callback m_writeCallback;
    ~Channel()
    {
        if (m_fd > 0)
            close(m_fd);
    }

private:
    // 文件描述符
    int m_fd;
    // 监听的事件
    FDEvent m_events;
};
