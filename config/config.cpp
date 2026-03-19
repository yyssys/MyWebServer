#include "config.h"
#include <getopt.h>
#include <iostream>
#include <thread>
#include <unistd.h>

Config::Config()
    : listenPort(10000),
      enableLogging(true),
      logWriteMode(LogWriteMode::Sync),
      sqlConnCount(8),
      workerThreadCount(std::thread::hardware_concurrency()),
      reactorType(ReactorType::Epoll),
      triggerMode(TriggerMode::LT)
{
    if (workerThreadCount <= 0)
    {
        workerThreadCount = 0;
    }
    char buffer[1024];

    if (getcwd(buffer, sizeof(buffer)) != nullptr)
    {
        std::string server_path(buffer);

        server_path += "/source";

        rootPath = server_path;
    }
}
void Config::parseCommandLine(int argc, char *argv[])
{
    const option longOptions[] = {
        {"port", required_argument, nullptr, 'p'},
        {"log-enable", required_argument, nullptr, 'l'},
        {"log-mode", required_argument, nullptr, 'm'},
        {"sql-connections", required_argument, nullptr, 's'},
        {"threads", required_argument, nullptr, 't'},
        {"reactor", required_argument, nullptr, 'r'},
        {"trigger", required_argument, nullptr, 'g'},
        {"help", no_argument, nullptr, 'h'},
        {nullptr, 0, nullptr, 0}};

    const char *shortOptions = "p:l:m:s:t:r:g:h";
    optind = 1;

    while (true)
    {
        const int opt = getopt_long(argc, argv, shortOptions, longOptions, nullptr);
        if (opt == -1)
        {
            break;
        }

        switch (opt)
        {
        case 'p':
            listenPort = std::atoi(optarg);
            break;
        case 'l':
            enableLogging = parseBoolOption(optarg);
            break;
        case 'm':
            logWriteMode = parseLogWriteMode(optarg);
            break;
        case 's':
            sqlConnCount = std::atoi(optarg);
            break;
        case 't':
            workerThreadCount = std::atoi(optarg);
            break;
        case 'r':
            reactorType = parseReactorType(optarg);
            break;
        case 'g':
            triggerMode = parseTriggerMode(optarg);
            break;
        case 'h':
            printUsage(argv[0]);
            std::exit(0);
        default:
            printUsage(argv[0]);
            throw std::invalid_argument("invalid command line arguments");
        }
    }
    // poll和select没有边沿触发模式
    if (reactorType != ReactorType::Epoll)
    {
        triggerMode = TriggerMode::LT;
    }
}

void Config::printUsage(const char *programName) const
{
    std::cout
        << "Usage: " << programName << " [options]\n"
        << "  -p, --port <port>                 Listen port, default 10000\n"
        << "  -l, --log-enable <0|1>           Enable logging, default 1\n"
        << "  -m, --log-mode <sync|async>      Log write mode, default sync\n"
        << "  -s, --sql-connections <count>    Database connection count, default 8\n"
        << "  -t, --threads <count>            Worker thread count, default hardware_concurrency\n"
        << "  -r, --reactor <epoll|poll|select> Reactor backend, default epoll\n"
        << "  -g, --trigger <lt|et>            epoll trigger mode, default lt\n"
        << "  -h, --help                       Show this help message\n";
}

bool Config::parseBoolOption(const char *value)
{
    const std::string text(value);
    if (text == "1" || text == "true" || text == "on" || text == "yes")
    {
        return true;
    }
    if (text == "0" || text == "false" || text == "off" || text == "no")
    {
        return false;
    }
    throw std::invalid_argument("--log-enable expects '0' or '1'");
}

LogWriteMode Config::parseLogWriteMode(const char *value)
{
    const std::string text(value);
    if (text == "sync")
    {
        return LogWriteMode::Sync;
    }
    if (text == "async")
    {
        return LogWriteMode::Async;
    }
    throw std::invalid_argument("--log-mode expects 'sync' or 'async'");
}

ReactorType Config::parseReactorType(const char *value)
{
    const std::string text(value);
    if (text == "epoll")
    {
        return ReactorType::Epoll;
    }
    if (text == "poll")
    {
        return ReactorType::Poll;
    }
    if (text == "select")
    {
        return ReactorType::Select;
    }
    throw std::invalid_argument("--reactor expects 'epoll', 'poll' or 'select'");
}

TriggerMode Config::parseTriggerMode(const char *value)
{
    const std::string text(value);
    if (text == "lt")
    {
        return TriggerMode::LT;
    }
    if (text == "et")
    {
        return TriggerMode::ET;
    }
    throw std::invalid_argument("--trigger expects 'lt' or 'et'");
}
