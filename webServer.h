#pragma once
#include <sys/socket.h>
#include <arpa/inet.h>
#include "config/config.h"
#include "dispatcher/dispatcher.h"
#include "threadpool/threadpool.h"
class webServer
{
public:
    webServer(const Config &config);
    ~webServer();
    void run();

private:
    void acceptConnection();
    // 初始化监听
    void setListen();
    int m_lfd; // 监听的文件描述符
    Dispatcher *m_mainDispatcher;
    ThreadPool *m_threadPool;
    Config m_config;
    bool is_use_log;
};
