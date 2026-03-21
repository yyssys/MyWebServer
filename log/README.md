
## 模块组成

- `log.h / log.cpp`：日志单例、文件输出、异步线程管理
- `block_queue.h`：异步日志使用的阻塞队列

## 作用

- 按等级输出日志
- 将日志写入文件
- 在异步模式下通过阻塞队列解耦业务线程和磁盘写入
- 按日期切分日志文件

## 使用方式

- 程序启动时在 `main.cpp` 中调用 `Log::getInstance()->init(config)`
- 业务代码通过 `LOG_INFO`、`LOG_ERROR`、`LOG_PRINT` 宏写日志

