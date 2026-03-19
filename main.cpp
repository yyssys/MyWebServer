#include "webServer.h"
#include "config/config.h"
#include "sql_conn_pool/sql_conn_pool.h"
#include <signal.h>
int main(int argc, char *argv[])
{
    signal(SIGPIPE, SIG_IGN);
    // 数据库配置
    const std::string url = "127.0.0.1:3306";
    const std::string root = "root";
    const std::string passwd = "110120";
    const std::string dbname = "ys";


    // 命令行解析
    Config config;
    config.parseCommandLine(argc, argv);
    // 初始化日志系统
    Log::getInstance()->init(config);
    // 初始化数据库连接池
    MysqlConnPool::getInstance()->init(url, root, passwd, dbname, config.sqlConnCount);
    // 启动服务器
    webServer server(config);
    server.run();

    return 0;
}
