#include "buffer.h"
#include <sys/socket.h>
#include <sys/uio.h>
#include <cstdlib>
#include <cstring>
Buffer::Buffer(int size) : capacity(size), readPos(0), writePos(0)
{
    data = (char *)malloc(size);
    memset(data, 0, size);
}

Buffer::~Buffer()
{
    if (data != nullptr)
    {
        free(data);
    }
}

bool Buffer::extendRoom(int size)
{
    // 1. 内存够用 - 不需要扩容
    if (writeAbleSize() >= size)
    {
        return true;
    }
    // 2. 内存需要合并才够用 - 不需要扩容
    // 剩余的可写的内存 + 已读的内存 > size
    else if (readPos + writeAbleSize() >= size)
    {
        // 得到未读的内存大小
        int readable = readAbleSize();
        // 移动内存
        memmove(data, data + readPos, readable);
        // 更新位置
        readPos = 0;
        writePos = readable;
        return true;
    }
    // 3. 内存不够用 - 扩容
    else
    {
        void *temp = realloc(data, capacity + size);
        if (temp == NULL)
        {
            return false;
        }
        memset((char *)temp + capacity, 0, size);
        // 更新数据
        data = static_cast<char *>(temp);
        capacity += size;
        return true;
    }
}

int Buffer::appendData(const char *s, int size)
{
    if (s == nullptr || size <= 0)
    {
        return -1;
    }

    if (!extendRoom(size))
    {
        return -1;
    }
    memcpy(data + writePos, s, size);
    writePos += size;
    return 0;
}

int Buffer::appendData(const std::string &data)
{
    return appendData(data.data(), static_cast<int>(data.size()));
}

char *Buffer::findCRLF()
{
    char *ptr = (char *)memmem(data + readPos, readAbleSize(), "\r\n", 2);
    return ptr;
}

void Buffer::updateReadPos(int count)
{
    if (count <= 0)
    {
        return;
    }
    readPos += count;
}

void Buffer::updateWritePos(int count)
{
    if (count <= 0)
    {
        return;
    }
    writePos += count;
}

void Buffer::retrieveAll()
{
    readPos = 0;
    writePos = 0;
}
