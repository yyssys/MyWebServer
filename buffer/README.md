# buffer 模块

`buffer` 模块提供一个简单的可扩容字节缓冲区，主要给 `HttpConnection` 的读缓冲区和写缓冲区使用。

## 核心职责

- 保存从 socket 读取到的原始数据
- 保存待发送的响应头数据
- 维护读指针 `readPos` 和写指针 `writePos`
- 在空间不足时执行内存整理或扩容

## 核心类

### `Buffer`

主要接口：

- `appendData()`：向缓冲区追加数据
- `extendRoom()`：空间不足时扩容或整理
- `findCRLF()`：查找 HTTP 行结束符 `\r\n`
- `updateReadPos()` / `updateWritePos()`：推进读写指针
- `retrieveAll()`：清空缓冲区状态

## 实现说明

- 底层使用一块连续内存
- 优先复用已读空间，只有在确实不足时才 `realloc`
- 设计比较直接，适合当前项目这种单连接单缓冲区的场景

## 依赖关系

- 被 `http_conn/` 直接使用
