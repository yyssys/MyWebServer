#pragma once
#include <string>

class Buffer
{
public:
    Buffer(int size = 10240);
    ~Buffer();

    // 扩容
    void extendRoom(int size);
    // 得到剩余的可写的内存容量
    int writeAbleSize()
    {
        return capacity - writePos;
    }
    // 得到剩余的可读的内存容量
    int readAbleSize()
    {
        return writePos - readPos;
    }
    // 把读到的数据添加到缓存中
    int appendData(const char *data, int size);
    int appendData(const std::string &data);
    int sendData(int socket);

    // 找到\r\n在数据块中的位置, 返回该位置
    char *findCRLF();

    void updateReadPos(int count);
    void retrieveAll();

public:
    char *data;
    int capacity;
    int readPos;
    int writePos;
};
