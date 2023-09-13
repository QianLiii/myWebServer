#include "thread_pool.hh"
#include <iostream>


Thread_Pool::Thread_Pool(size_t thread_num) {
    for(int i = 0;i<thread_num;++i) {
        std::thread(&Thread_Pool::_thread_loop, this).detach();
    }
}

Thread_Pool::~Thread_Pool() {
    std::lock_guard<std::mutex> lck_gd(_mtx);
    _is_close = true;
    _con_var.notify_all();
}

void Thread_Pool::push_task(std::function<void()> &&fun) {
    // 修改任务队列前要加锁
    std::lock_guard<std::mutex> lck_gd(_mtx);
    _tasks.push(std::forward<std::function<void()>>(fun));
    _con_var.notify_one();
}

void Thread_Pool::_thread_loop() {
    std::unique_lock<std::mutex> unq_lock(_mtx);    // 配合条件变量使用
    while(!_is_close) {
        // 保证在判断empty前持有锁，所以不会出现判断结果和实际不一致的情况
        // !_tasks.empty()起到相当于_con_var.wait()的谓词的作用，如果条件变量被虚假唤醒了，但此时任务还为空，就再次阻塞等待通知。
        if(!_tasks.empty()) {
            // 取一个任务
            auto task = std::move(_tasks.front());
            _tasks.pop();
            // 释放锁
            unq_lock.unlock();
            // 执行任务
            // std::cout<<"this is thread "<<_gei_id()<<" doing the task.\n";
            try {
                task();
            } catch (Serv_Exception e) {
                std::cout<<e.what();
                exit(1);
            }
            // 获取锁
            unq_lock.lock();
        }
        else {
            // 释放锁，阻塞自己，等待notify_one()
            // 任务队列不为空的情况下不会调用wait，所以不用考虑唤醒丢失的情况（如果发出一个notify_one时没有线程在wait，那么肯定处于if为真的循环中）
            _con_var.wait(unq_lock);
            // _con_var.wait(unq_lock, [&](){return !_tasks.empty();});
        }
    }
    std::cout<<"thread "<<std::this_thread::get_id()<<" will be destroyed very soon.\n";
}