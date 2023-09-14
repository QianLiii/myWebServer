#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <thread>
#include <mutex>
#include <queue>
#include <functional>
#include <condition_variable>
#include <iostream>
#include "serv_exception.hh"

#define THREAD_NUM 8

class Thread_Pool {

private:
    Thread_Pool();
    ~Thread_Pool();
public:
    Thread_Pool(const Thread_Pool&) = delete;
    Thread_Pool& operator=(const Thread_Pool&) = delete;
    
    static Thread_Pool& get_pool() {
        static Thread_Pool pool;
        return pool;
    }
    
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