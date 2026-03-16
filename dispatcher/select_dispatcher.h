#pragma once
#include <sys/select.h>
#include "dispatcher.h"

class SelectDispatcher : public Dispatcher
{
public:
    SelectDispatcher(bool uselog) : Dispatcher(uselog)
    {
        initWakeupChannel();
    }
    ~SelectDispatcher() {}

    // 添加
    void add(Channel *channel) override;
    // 删除
    void remove(Channel *channel) override;
    // 修改
    void modify(Channel *channel) override;
    // 事件监测
    void dispatch(int timeout = 2) override;

private:
    void setFdSet(Channel *channel);
    void clearFdSet(Channel *channel);
    fd_set m_readSet{};
    fd_set m_writeSet{};
};
