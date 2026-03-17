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
        return m_capacity - m_writePos;
    }
    // 得到剩余的可读的内存容量
    int readAbleSize()
    {
        return m_writePos - m_readPos;
    }
    // 把读到的数据添加到缓存中
    int appendData(const char *data, int size);

    // 根据\r\n取出一行, 找到其在数据块中的位置, 返回该位置
    char *findCRLF();

    // 得到读数据的起始位置
    char *readStartPos()
    {
        return m_data + m_readPos;
    }
    // 得到写数据的起始位置
    char *writeStartPos()
    {
        return m_data + m_writePos;
    }
    void updateReadPos(int count)
    {
        m_readPos += count;
    }
    void updateWritePos(int count)
    {
        m_writePos += count;
    }

private:
    char *m_data;
    int m_capacity;
    int m_readPos;
    int m_writePos;
};
