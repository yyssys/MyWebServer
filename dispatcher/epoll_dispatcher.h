#pragma once
#include <sys/epoll.h>
#include "dispatcher.h"

class EpollDispatcher : public Dispatcher
{
public:
    EpollDispatcher(bool uselog = true, int triggerMode = 1);
    ~EpollDispatcher();

    // 添加
    void add(Channel *channel) override;
    // 删除
    void remove(Channel *channel) override;
    // 修改
    void modify(Channel *channel) override;
    // 事件监测
    void dispatch(int timeout = 2) override;

private:
    int updateEpoll(Channel *channel, int op);

    int m_epollFd;
    int m_triggerMode; // 触发模式，默认是ET
    struct epoll_event m_epollEvents[1024];
};
