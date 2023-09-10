#ifndef EPOLLER_H
#define EPOLLER_H

#include <sys/epoll.h>
#include <fcntl.h>
#include <unistd.h>
#include <vector>
#include <exception>

class Epoller {
public:
    Epoller(size_t max_events);
    ~Epoller();
    bool add_fd(int sock_fd, uint32_t event);
    bool mod_fd(int sock_fd, uint32_t event);
    bool del_fd(int sock_fd);
    int wait(int timeout);
    int get_sock_fd(size_t i) const;
    uint32_t get_event(size_t i) const;
private:
    int _epoll_fd;
    std::vector<epoll_event> _events;
};

#endif