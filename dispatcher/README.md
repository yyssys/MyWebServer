## dispatcher组成

- `dispatcher.h / dispatcher.cpp`
  作用：定义抽象基类 `Dispatcher`，封装通用逻辑
- `epoll_dispatcher.*`
  作用：基于 `epoll` 的实现
- `poll_dispatcher.*`
  作用：基于 `poll` 的实现
- `select_dispatcher.*`
  作用：基于 `select` 的实现

## `Dispatcher` 的职责

- 管理 `Channel`
- 提供 `add` / `remove` / `modify` 抽象接口
- 维护跨线程任务队列，支持主线程向子反应堆投递任务
- 通过 `socketpair` 实现唤醒机制
- 在启用定时器时，通过 `timerfd` 定期触发连接超时检查

## 超时管理

- 子反应堆会创建 `timerfd`
- 每 5 秒触发一次读事件
- `handleAlarm()` 只负责把 `timeout` 标志置为 `true`
- 每轮 `dispatch()` 结束后，如果检测到超时标志，就调用 `processTimeout()`
- `processTimeout()` 会从 `timer` 模块维护的链表中取出已过期连接，并触发关闭回调

主反应堆不会启用这个定时器，它只负责监听新连接。

## 特点

- 主反应堆负责接收连接
- 子反应堆负责连接生命周期管理
- 不同 I/O 复用实现复用一套公共调度逻辑

