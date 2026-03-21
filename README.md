# MyWebServer

一个基于 C++11 实现的轻量级 Web 服务器项目，核心采用 Reactor + 线程池 模型。

## 整体流程

1. `main.cpp` 解析命令行参数，初始化日志和数据库连接池。
2. `webServer` 创建主反应堆和线程池。
3. 主反应堆负责监听 `listen fd`(线程数量为0时主反应堆既负责`lfd`也负责`cfd`)，接收到新连接后按轮询分发给子反应堆。
4. 子反应堆负责连接读写、HTTP 解析、响应发送和连接超时管理。
5. `HttpConnection` 处理具体的 HTTP 请求和响应逻辑。

## 模块

- `buffer/`：读写缓冲区
- `channel/`：文件描述符事件封装
- `config/`：命令行参数与运行配置
- `dispatcher/`：Reactor 抽象及 `epoll/poll/select` 实现
- `http_conn/`：HTTP 连接处理
- `log/`：同步/异步日志系统
- `sql_conn_pool/`：MySQL 连接池
- `threadpool/`：工作线程与线程池
- `timer/`：连接超时链表
- `source/`：静态页面资源

## 特点

- 支持 `epoll`、`poll`、`select` 三种 I/O 复用
- 支持 LT / ET 触发模式
- 子反应堆内部使用 `timerfd` 每 5 秒检查一次空闲连接
- 支持基础的登录、注册和静态文件访问
- 使用 `fmt` 做日志和响应字符串格式化
