#include "sql_conn_pool.h"

MysqlConnPool::MysqlConnPool()
    : m_driver(nullptr),
      m_poolSize(0),
      m_initialized(false),
      m_shuttingDown(false)
{
}

MysqlConnPool::~MysqlConnPool()
{
    shutdown();
}

MysqlConnPool *MysqlConnPool::getInstance()
{
    static MysqlConnPool inst;
    return &inst;
}

void MysqlConnPool::init(const std::string &url,
                         const std::string &user,
                         const std::string &password,
                         const std::string &dbname,
                         int poolSize)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_initialized)
    {
        return;
    }

    if (poolSize == 0)
    {
        throw std::invalid_argument("poolSize cannot be 0");
    }

    m_url = url;
    m_user = user;
    m_password = password;
    m_dbname = dbname;
    m_poolSize = poolSize;
    m_shuttingDown = false;

    m_driver = sql::mysql::get_mysql_driver_instance();
    if (!m_driver)
    {
        throw std::runtime_error("failed to get mysql driver instance");
    }

    for (int i = 0; i < m_poolSize; ++i)
    {
        sql::Connection *conn = createConnection();
        m_pool.push(conn);
    }
    m_initialized = true;
}

sql::Connection *MysqlConnPool::createConnection()
{
    sql::Connection *conn = m_driver->connect(m_url, m_user, m_password);
    conn->setSchema(m_dbname);
    conn->setAutoCommit(true);
    return conn;
}

bool MysqlConnPool::isConnectionValid(sql::Connection *conn)
{
    if (conn == nullptr)
    {
        return false;
    }
    if (conn->isClosed())
    {
        return false;
    }
    return conn->isValid();
}

auto MysqlConnPool::acquire() -> std::shared_ptr<sql::Connection>
{
    std::unique_lock<std::mutex> lock(m_mutex);

    if (!m_initialized || m_shuttingDown)
    {
        return std::shared_ptr<sql::Connection>();
    }

    m_cond.wait(lock, [this]()
                { return m_shuttingDown || !m_pool.empty(); });
    // 连接池关闭了返回空指针
    if (m_shuttingDown)
    {
        return std::shared_ptr<sql::Connection>();
    }

    sql::Connection *raw = m_pool.front();
    m_pool.pop();

    lock.unlock();

    // 借出前检查连接是否有效，失效就重建
    if (!isConnectionValid(raw))
    {
        delete raw;
        raw = createConnection();
    }

    // 自定义 deleter：shared_ptr 析构时自动归还连接
    return std::shared_ptr<sql::Connection>(raw, [this](sql::Connection *conn)
                                            { this->returnConnection(conn); });
}

void MysqlConnPool::returnConnection(sql::Connection *conn)
{
    if (conn == nullptr)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_shuttingDown || !m_initialized)
    {
        delete conn;
        return;
    }

    m_pool.push(conn);
    m_cond.notify_one();
}

void MysqlConnPool::shutdown()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized)
    {
        return;
    }

    m_shuttingDown = true;

    while (!m_pool.empty())
    {
        sql::Connection *conn = m_pool.front();
        m_pool.pop();

        if (conn && !conn->isClosed())
        {
            conn->close();
        }

        delete conn;
    }

    m_initialized = false;
    m_cond.notify_all();
}
