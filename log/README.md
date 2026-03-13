异步/同步日志系统
=
* 阻塞队列模块+日志模块构成
* 单例模式
* 按天划分日志文件
* 默认`异步`
* 调用`init`方法时指定阻塞队列长度`<1`时为`同步`
* C++11新特性参数包、`fmt:format()`格式化可变参数为字符串
*  `std:format()`为C++20新特性，本项目为C++11，下载`fmt`静态库：
    ```shell
    sudo apt update
    sudo apt install libfmt-dev
    ```
