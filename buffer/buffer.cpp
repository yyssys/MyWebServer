#include "buffer.h"
#include <sys/uio.h>
#include <cstring>
Buffer::Buffer(int size) : m_capacity(size), m_readPos(0), m_writePos(0)
{
    m_data = (char *)malloc(size);
    memset(m_data, 0, size);
}

Buffer::~Buffer()
{
    if (m_data != nullptr)
    {
        free(m_data);
    }
}

void Buffer::extendRoom(int size)
{
    // 1. 内存够用 - 不需要扩容
    if (writeAbleSize() >= size)
    {
        return;
    }
    // 2. 内存需要合并才够用 - 不需要扩容
    // 剩余的可写的内存 + 已读的内存 > size
    else if (m_readPos + writeAbleSize() >= size)
    {
        // 得到未读的内存大小
        int readable = readAbleSize();
        // 移动内存
        memcpy(m_data, m_data + m_readPos, readable);
        // 更新位置
        m_readPos = 0;
        m_writePos = readable;
    }
    // 3. 内存不够用 - 扩容
    else
    {
        void *temp = realloc(m_data, m_capacity + size);
        if (temp == NULL)
        {
            return; // 失败了
        }
        memset((char *)temp + m_capacity, 0, size);
        // 更新数据
        m_data = static_cast<char *>(temp);
        m_capacity += size;
    }
}

int Buffer::appendData(const char *data, int size)
{
    if (data == nullptr || size <= 0)
    {
        return -1;
    }

    extendRoom(size);
    memcpy(m_data + m_writePos, data, size);
    m_writePos += size;
    return 0;
}

char *Buffer::findCRLF()
{
    char *ptr = (char *)memmem(m_data + m_readPos, readAbleSize(), "\r\n", 2);
    return ptr;
}
