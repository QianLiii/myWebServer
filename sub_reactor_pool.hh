#ifndef SUB_REACTOR_POOL_H
#define SUB_REACTOR_POOL_H

#include <sys/socket.h>
#include <arpa/inet.h>
#include <memory>
#include <shared_mutex>
#include <errno.h>
#include <iostream>
#include<queue>
#include <mutex>

#include "thread_pool.hh"
#include "epoller.hh"
#include "timer.hh"
#include "http_connection.hh"

class Sub_Reactor_Pool {
public:
    Sub_Reactor_Pool(size_t thread_num, int64_t timeout);

    void push_client_fd(int fd);

private:
    void _loop();
    void _set_nonblocking(int fd);
    void _close_sock(int fd);
    void _read(int fd);
    void _write(int fd);
    void _accecpt_new_sock();
private:
    std::mutex _fd_queue_mtx;
    std::queue<int> _client_fds;


    int64_t _timeout;
    bool _is_close{false};

    std::unique_ptr<Epoller> _epoller;
    std::map<int, std::shared_ptr<Http_Connection>> _connections;
    std::unique_ptr<Timer> _timer;
    
    // 重要：：多个工作线程和主线程可能出现同时访问以上成员的情况，所以需要读写锁
    std::shared_mutex _conn_mtx;
};


















#endif