#pragma once
#include <sys/epoll.h>
#include "dispatcher.h"

class EpollDispatcher : public Dispatcher
{
public:
    EpollDispatcher(const Config &config);
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
    struct epoll_event m_epollEvents[1024];
};
