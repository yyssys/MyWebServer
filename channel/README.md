# channel 模块

`channel` 模块用于把一个文件描述符及其感兴趣事件、回调函数绑定在一起，是 Reactor 模型中的事件对象。

## 核心职责

- 封装 `fd`
- 记录当前监听的是读事件、写事件还是两者
- 保存读、写、关闭三类回调
- 对外提供统一的 `handleRead()` / `handleWrite()` / `handleClose()` 接口

## 核心类型

### `FDEvent`

表示文件描述符关注的事件：

- `ReadEvent`
- `WriteEvent`
- `None`

项目中对它实现了按位运算，便于组合读写事件。

### `Channel`

`Channel` 是 Dispatcher 管理的最小事件单元。

## 实现说明

- `Channel` 析构时会关闭自身持有的 `fd`
- `HttpConnection` 会为客户端连接创建 `Channel`
- `Dispatcher` 会把 `Channel` 注册到具体的 I/O 复用后端

## 依赖关系

- 被 `dispatcher/`
- 被 `http_conn/`
- 被 `webServer.cpp`
