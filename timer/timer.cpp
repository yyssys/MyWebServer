#include "timer.h"

void Timer::addOrUpdate(int fd, time_t expireTime)
{
    auto iter = m_index.find(fd);
    if (iter != m_index.end())
    {
        m_list.erase(iter->second);
        m_index.erase(iter);
    }

    auto insertPos = m_list.end();
    for (auto listIter = m_list.begin(); listIter != m_list.end(); ++listIter)
    {
        if (listIter->expireTime > expireTime)
        {
            insertPos = listIter;
            break;
        }
    }

    auto newIter = m_list.insert(insertPos, timer_unit{fd, expireTime});
    m_index[fd] = newIter;
}

void Timer::remove(int fd)
{
    auto iter = m_index.find(fd);
    if (iter == m_index.end())
    {
        return;
    }

    m_list.erase(iter->second);
    m_index.erase(iter);
}

std::vector<int> Timer::takeExpired(time_t now)
{
    std::vector<int> expiredFds;
    while (!m_list.empty())
    {
        const timer_unit &unit = m_list.front();
        if (unit.expireTime > now)
        {
            break;
        }

        expiredFds.push_back(unit.fd);
        m_index.erase(unit.fd);
        m_list.pop_front();
    }
    return expiredFds;
}
