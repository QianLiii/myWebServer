#include "epoller.hh"

#include <iostream>

// 初始化epoller
Epoller::Epoller(size_t max_events) : _epoll_fd(epoll_create(128)), _events(max_events) {
}

Epoller::~Epoller() {
    close(_epoll_fd);
}

// 封装epoll的功能

// 注册监视sock_fd的event事件
bool Epoller::add_fd(int sock_fd, uint32_t event) {
    epoll_event ev{};
    ev.events = event;
    ev.data.fd = sock_fd;
    return 0 == epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, sock_fd, &ev);
}

// 修改事件
bool Epoller::mod_fd(int sock_fd, uint32_t event) {
    epoll_event ev{};
    ev.events = event;
    ev.data.fd = sock_fd;
    return 0 == epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, sock_fd, &ev);
}

// 删除事件
bool Epoller::del_fd(int sock_fd) {
    epoll_event ev{};
    return 0 == epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, sock_fd, &ev);
}

// 封装epoll_wait
int Epoller::wait(int timeout) {
    return epoll_wait(_epoll_fd, &_events[0], _events.size(), timeout);
}

// 获取发生事件的fd
int Epoller::get_sock_fd(size_t i) const {
    if(i < 0 || i >= _events.size()) {
        throw std::exception();
    }
    return _events[i].data.fd;
}

// 获取发生的事件
uint32_t Epoller::get_event(size_t i) const {
    if(i < 0 || i >= _events.size()) {
        throw std::exception();
    }
    return _events[i].events;
}