#pragma once
#include <memory>
#include <mutex>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unordered_map>
#include "config/config.h"
#include "dispatcher/dispatcher.h"
#include "http_conn/http_conn.h"
#include "threadpool/threadpool.h"
class webServer
{
public:
    webServer(const Config &config);
    ~webServer();
    void run();

private:
    void acceptConnection();
    void removeConnection(int fd);
    // 初始化监听
    void setListen();
    int m_lfd; // 监听的文件描述符
    Channel *m_listenChannel;
    Dispatcher *m_mainDispatcher;
    ThreadPool *m_threadPool;
    std::mutex m_connectionMutex;
    std::unordered_map<int, std::unique_ptr<HttpConnection>> m_connections;
    Config m_config;
    bool is_use_log;
};
