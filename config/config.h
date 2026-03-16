#pragma once

#include <string>

enum class LogWriteMode
{
    Sync = 0,
    Async = 1
};

enum class ReactorType
{
    Epoll = 0,
    Poll = 1,
    Select = 2
};

enum class TriggerMode
{
    LevelTriggered = 0,
    EdgeTriggered = 1
};

class Config
{
public:
    Config();

    void parseCommandLine(int argc, char *argv[]);
    void printUsage(const char *programName) const;

    int listenPort;
    bool enableLogging;
    LogWriteMode logWriteMode;
    int logQueueSize;
    int databaseConnectionCount;
    int workerThreadCount;
    ReactorType reactorType;
    TriggerMode triggerMode;

private:
    bool parseBoolOption(const char *value);
    LogWriteMode parseLogWriteMode(const char *value);
    ReactorType parseReactorType(const char *value);
    TriggerMode parseTriggerMode(const char *value);
};
