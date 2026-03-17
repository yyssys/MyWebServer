#include "webServer.h"

webServer::webServer(const Config &config)
    : m_mainDispatcher(nullptr),
      is_use_log(config.enableLogging),
      m_threadPool(nullptr),
      m_config(config)
{
    setListen();
}

webServer::~webServer()
{
    if (m_mainDispatcher)
        delete m_mainDispatcher;
    if (m_threadPool)
        delete m_threadPool;
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
    Channel *channel = new Channel(m_lfd, FDEvent::ReadEvent, std::bind(&webServer::acceptConnection, this), nullptr);
    // 将监听所用的channel加入主反应堆
    m_mainDispatcher->add(channel);
}

void webServer::acceptConnection()
{
    struct sockaddr_in client_address;
    socklen_t client_addrlength = sizeof(client_address);
    int cfd = accept(m_lfd, (struct sockaddr *)&client_address, &client_addrlength);
    if (cfd < 0)
    {
        LOG_ERROR("accept failed, errno: %d, errmsg: %s", errno, strerror(errno));
        return;
    }
    char clientIp[64] = {0};
    inet_ntop(AF_INET, &client_address.sin_addr, clientIp, sizeof(clientIp));
    LOG_INFO("与客户端建立连接,客户端IP: %s,端口: %d", clientIp, ntohs(client_address.sin_port));
    // 从线程池中取出一个反应堆实例去处理这个cfd
    Dispatcher *dispatcher = m_threadPool->getDispatcher();
    // 根据反应堆和通信fd创建httpConn
    
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
