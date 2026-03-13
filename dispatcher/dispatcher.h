#pragma once
#include <unistd.h>
#include "log/log.h"
#include "common/channel.h"
class Dispatcher
{
public:
    Dispatcher() = default;
    // 添加
    virtual void add(Channel &Channel){};
    // 删除
    virtual void remove(){};
    // 修改
    virtual void modify(){};
    // 事件监测
    virtual void dispatch(int timeout = 2){}; // 单位: s
    
    virtual ~Dispatcher() {}

private:
};