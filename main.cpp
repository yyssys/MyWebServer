#include "webServer.h"
#include "config/config.h"
int main(int argc, char *argv[])
{
    // 命令行解析
    Config config;
    config.parseCommandLine(argc, argv);
    // 初始化日志系统
    Log::getInstance()->init(config);

    // 启动服务器
    webServer server(config);
    server.run();

    return 0;
}
