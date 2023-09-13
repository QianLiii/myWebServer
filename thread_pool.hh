#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <thread>
#include <mutex>
#include <queue>
#include <functional>
#include <condition_variable>
#include <iostream>
#include "serv_exception.hh"

class Thread_Pool {
public:
    Thread_Pool(size_t thread_num);
    ~Thread_Pool();
    void push_task(std::function<void()> &&fun);
private:
    std::mutex _mtx;
    bool _is_close{false};
    std::condition_variable _con_var;
    std::queue<std::function<void()>> _tasks;

private:
    void _thread_loop();
    std::thread::id _gei_id() const {return std::this_thread::get_id();}
};

#endif