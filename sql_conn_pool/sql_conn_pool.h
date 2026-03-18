#pragma once
#include <mysql_driver.h>
#include <cppconn/connection.h>
#include <cppconn/statement.h>

#include <memory>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <string>

class MysqlConnPool
{
public:
    static MysqlConnPool *getInstance();

    MysqlConnPool(const MysqlConnPool &) = delete;
    MysqlConnPool &operator=(const MysqlConnPool &) = delete;

    // 初始化连接池，只允许程序启动时调用一次
    void init(const std::string &url,
              const std::string &user,
              const std::string &password,
              const std::string &dbname,
              int poolSize);

    // 阻塞获取连接，超时返回空指针
    std::shared_ptr<sql::Connection> acquire();

    // 关闭连接池
    void shutdown();

private:
    MysqlConnPool();
    ~MysqlConnPool();
    // 创建一个数据库连接并返回
    sql::Connection *createConnection();
    // 判断数据库连接是否有效
    bool isConnectionValid(sql::Connection *conn);
    // 归还数据库连接
    void returnConnection(sql::Connection *conn);

private:
    sql::mysql::MySQL_Driver *m_driver;

    std::string m_url;
    std::string m_user;
    std::string m_password;
    std::string m_dbname;

    int m_poolSize;
    bool m_initialized;
    bool m_shuttingDown;

    std::queue<sql::Connection *> m_pool;
    std::mutex m_mutex;
    std::condition_variable m_cond;
};