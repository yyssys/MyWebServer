## http_conn 模块

负责单个 HTTP 连接的完整生命周期。

- 读取客户端请求
- 解析 HTTP 请求行、请求头、请求体
- 处理登录、注册等表单请求
- 组织 HTTP 响应头和响应体
- 发送静态资源文件
- 连接关闭时回收资源并通知上层删除连接对象

一个 `HttpConnection` 对应一个客户端连接，内部主要包含：

- `Channel`
- 读缓冲区 `Buffer`
- 写缓冲区 `Buffer`
- 当前请求解析状态
- 文件映射信息
- 关闭回调

## 处理流程

1. `CallbackProcessRead()` 读取 socket 数据
2. `process_read()` 逐步解析请求
3. `prepareResponse()` 准备响应内容
4. `process_write()` 组织响应头和文件数据
5. `CallbackProcessWrite()` 使用 `writev` 发送响应

## 当前支持

- `GET`
- `POST`
- 基础的登录和注册逻辑
- 静态 HTML 文件返回
- `keep-alive`

