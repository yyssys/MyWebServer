#pragma once
#include <ctime>
#include <list>
#include <unordered_map>
#include <vector>

struct timer_unit
{
    int fd;
    time_t expireTime;
};

class Timer
{
public:
    void addOrUpdate(int fd, time_t expireTime);
    void remove(int fd);
    std::vector<int> takeExpired(time_t now);

private:
    std::list<timer_unit> m_list;
    std::unordered_map<int, std::list<timer_unit>::iterator> m_index;
};
