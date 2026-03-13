#pragma once
#include <functional>
// 定义文件描述符的读写事件
enum class FDEvent : char
{
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
    using callback = std::function<void()>;

    Channel(int fd, FDEvent events, callback readFunc, callback writeFunc);

    // 是否监听写事件
    void setListenWriteEvent(bool flag);

    // 获取 fd
    int getfd()
    {
        return m_fd;
    }

    // 回调函数
    callback readCallback;
    callback writeCallback;

private:
    // 文件描述符
    int m_fd;
    // 监听的事件
    FDEvent m_events;
};