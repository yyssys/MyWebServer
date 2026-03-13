#pragma once
#include <sys/epoll.h>

#include "dispatcher.h"

class EpollDispatcher : public Dispatcher
{
public:
    EpollDispatcher();
    ~EpollDispatcher();

    // 添加
    void add(Channel &Channel) override;
    // 删除
    virtual void remove() override;
    // 修改
    virtual void modify() override;
    // 事件监测
    virtual void dispatch(int timeout = 2) override; // 单位: s

private:
    int m_epfd;
    struct epoll_event m_events[1024];
};