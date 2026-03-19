#include "webServer.h"
#include <cerrno>

webServer::webServer(const Config &config)
    : m_listenChannel(nullptr),
      m_mainDispatcher(nullptr),
      m_threadPool(nullptr),
      m_config(config),
      is_use_log(config.enableLogging)
{
    setListen();
}

webServer::~webServer()
{
    if (m_threadPool)
        delete m_threadPool;
    {
        std::lock_guard<std::mutex> lock(m_connectionMutex);
        m_connections.clear();
    }
    if (m_mainDispatcher)
        delete m_mainDispatcher;
    delete m_listenChannel;
}

void webServer::run()
{
    // 创建主反应堆
    switch (m_config.reactorType)
    {
    case ReactorType::Epoll:
        m_mainDispatcher = new EpollDispatcher(m_config);
        break;
    case ReactorType::Poll:
        m_mainDispatcher = new PollDispatcher(m_config);
        break;
    case ReactorType::Select:
        m_mainDispatcher = new SelectDispatcher(m_config);
        break;
    default:
        LOG_ERROR("反应堆模型选择错误");
        exit(0);
    }
    // 创建线程池
    m_threadPool = new ThreadPool(m_mainDispatcher, m_config);
    // 初始化监听fd的channel实例
    m_listenChannel = new Channel(
        m_lfd,
        FDEvent::ReadEvent,
        std::bind(&webServer::acceptConnection, this),
        nullptr,
        nullptr);
    // 将监听所用的channel加入主反应堆
    m_mainDispatcher->add(m_listenChannel);

    while (true)
    {
        m_mainDispatcher->dispatch();
    }
}

void webServer::acceptConnection()
{
    while (true)
    {
        struct sockaddr_in client_address{};
        socklen_t client_addrlength = sizeof(client_address);
        int cfd = accept(m_lfd, (struct sockaddr *)&client_address, &client_addrlength);
        if (cfd < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                return;
            }
            LOG_ERROR("accept failed, errno: {}, errmsg: {}", errno, strerror(errno));
            return;
        }
        char clientIp[64] = {0};
        inet_ntop(AF_INET, &client_address.sin_addr, clientIp, sizeof(clientIp));
        LOG_INFO("与客户端建立连接,客户端IP: {},端口: {}", clientIp, ntohs(client_address.sin_port));
        // 从线程池中取出一个反应堆实例去处理这个cfd
        Dispatcher *dispatcher = m_threadPool->getDispatcher();

        std::unique_ptr<HttpConnection> connection(new HttpConnection(
            m_config,
            cfd,
            dispatcher,
            std::bind(&webServer::removeConnection, this, std::placeholders::_1)));
        // 加入连接队列。加锁是因为有可能子线程正在调用removeConnection释放连接
        std::lock_guard<std::mutex> lock(m_connectionMutex);
        m_connections.emplace(cfd, std::move(connection));

        if (m_config.triggerMode != TriggerMode::ET)
        {
            return;
        }
    }
}
// 传给httpConnection的回调函数，用来断开连接时从webServer里面销毁连接
void webServer::removeConnection(int fd)
{
    std::lock_guard<std::mutex> lock(m_connectionMutex);
    m_connections.erase(fd);
}

void webServer::setListen()
{
    // 1. 创建监听的fd
    m_lfd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_lfd == -1)
    {
        LOG_ERROR("socket error");
        return;
    }
    // 2. 设置端口复用
    int opt = 1;
    int ret = setsockopt(m_lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (ret == -1)
    {
        LOG_ERROR("setsockopt error");
        return;
    }
    // 3. 绑定
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(m_config.listenPort);
    addr.sin_addr.s_addr = INADDR_ANY;
    ret = bind(m_lfd, (struct sockaddr *)&addr, sizeof(addr));
    if (ret == -1)
    {
        LOG_ERROR("bind error");
        return;
    }
    // 4. 设置监听
    ret = listen(m_lfd, 128);
    if (ret == -1)
    {
        LOG_ERROR("listen error");
        return;
    }
}
