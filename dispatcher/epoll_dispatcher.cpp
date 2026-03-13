#include "epoll_dispatcher.h"

EpollDispatcher::EpollDispatcher()
{
    m_epfd = epoll_create(10);
    if (m_epfd == -1)
    {
        perror("epoll_create");
        exit(0);
    }
}

EpollDispatcher::~EpollDispatcher()
{
    close(m_epfd);
}

void EpollDispatcher::add(Channel &Channel)
{
    
}
