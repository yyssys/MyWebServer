#pragma once
#include <sys/poll.h>
#include "dispatcher.h"

#define MaxNode 65536

class PollDispatcher : public Dispatcher
{
public:
    PollDispatcher(bool uselog);
    ~PollDispatcher() {}

    // 添加
    void add(Channel *channel) override;
    // 删除
    void remove(Channel *channel) override;
    // 修改
    void modify(Channel *channel) override;
    // 事件监测
    void dispatch(int timeout = 2) override;

private:
    int m_maxfd;
    struct pollfd m_pollfd[MaxNode];
};
