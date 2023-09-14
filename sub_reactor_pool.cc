#include "sub_reactor_pool.hh"

#define MAX_EVENTS 1024

Sub_Reactor_Pool::Sub_Reactor_Pool(size_t thread_num, int64_t timeout): _timeout(timeout), 
    _epoller(new Epoller(MAX_EVENTS)), _timer(new Timer()) {
    for(int i = 0;i<thread_num;++i) {
        std::thread(&Sub_Reactor_Pool::_loop, this).detach();
    }
}



void Sub_Reactor_Pool::push_client_fd(int fd) {
    std::lock_guard<std::mutex> lck_gd(_fd_queue_mtx);
    std::cout<<"fd added to queue."<<std::endl;
    _client_fds.push(fd);
}

void Sub_Reactor_Pool::_loop() {
    _accecpt_new_sock();
    // 距下一次超时的时间，-1表示无超时时间
    // int64_t next_timeout = -1;
    while(!_is_close) {
        std::cout<<"looping...\n"<<std::endl;
        // if(_timeout >= 0) {
            // next_timeout = _timer->tick();
        // }
        // int num_of_events = _epoller->wait(next_timeout);
        int num_of_events = _epoller->wait(_timeout);
        if(num_of_events < 0) {
            throw Serv_Exception("epoll wait fail\n");
        }
        for(int i = 0;i<num_of_events;++i) {
            int fd = _epoller->get_sock_fd(i);
            uint32_t ev = _epoller->get_event(i);
            // 处理断开连接
            if(ev & (EPOLLRDHUP | EPOLLERR | EPOLLHUP)) {
                _close_sock(fd);
            }
            // 分发读事件
            else if(ev & EPOLLIN) {
                std::cout<<"handle reading"<<std::endl;
                // 这里应该进一步封装
                _timer->update(fd, _timeout);
                // bind成员函数要传入this
                Thread_Pool::get_pool().push_task(std::bind(&Sub_Reactor_Pool::_read, this, fd));
            }
            // 分发写事件
            else if(ev & EPOLLOUT) {
                std::cout<<"handle writing"<<std::endl;
                _timer->update(fd, _timeout);
                Thread_Pool::get_pool().push_task(std::bind(&Sub_Reactor_Pool::_write, this, fd));
            }
        }
    }
}



void Sub_Reactor_Pool::_close_sock(int fd) {
    _epoller->del_fd(fd);
    {
        std::lock_guard<std::shared_mutex> guard(_conn_mtx);
        if(_connections.count(fd) > 0) {
            _connections.erase(fd);
        }
    }
    // 要主动关闭对应的定时器（不执行回调函数）
    _timer->erase(fd);
    close(fd);
}


// 从client socket读，非阻塞IO，边沿触发
void Sub_Reactor_Pool::_read(int fd) {
    // 调用connection的read
    // 应该处理几种情况：没有读完，注册一次读事件（需要保存下已经读的内容）；
    // 读完了，注册写事件；
    // 错误情况

    {
        std::shared_lock<std::shared_mutex> guard(_conn_mtx);
        if(_connections.count(fd) <= 0) {
            throw Serv_Exception("invalid connection.\n");
        }
    }
    std::cout<<"handle data from socket "<<fd<<"\n"<<std::endl;
    int ret;
    {
        std::shared_lock<std::shared_mutex> guard(_conn_mtx);
        ret = _connections[fd]->read();
    }
    if(ret == 0 || ret == ERR_READ_FAIL) {
        // 关闭连接
        std::cout<<"read failed or received FIN.\n"<<std::endl;
        _close_sock(fd);
    }
    // 如果read()遇到EAGAIN且read buffer不为空，就认为完成了数据的读取（认为一次http请求可以被一次性读完）
    else {
        bool ready;
        {
            std::shared_lock<std::shared_mutex> guard(_conn_mtx);
            ready = _connections[fd]->read_ready();
        }
        if(ready) {
            std::cout<<"read over. now wait for write.\n"<<std::endl;
            _epoller->mod_fd(fd, EPOLLOUT | EPOLLET | EPOLLONESHOT);
        }
        // 否则注册一次读事件
        else {
            std::cout<<"not fully read yet.\n"<<std::endl;
            _epoller->mod_fd(fd, EPOLLIN | EPOLLET | EPOLLONESHOT);
        }
    }
}

// 向client socket写
void Sub_Reactor_Pool::_write(int fd) {

    std::cout<<"send data to socket "<<fd<<"\n"<<std::endl;
    int ret;
    {
        std::shared_lock<std::shared_mutex> guard(_conn_mtx);
        ret = _connections[fd]->write();
    }
    // 写完后注册读事件
    bool keep_alive;
    {
        std::shared_lock<std::shared_mutex> guard(_conn_mtx);
        keep_alive = _connections[fd]->need_keep_alive();
    }
    if(ret == ERR_SUCCESS && keep_alive) {
        {
            std::shared_lock<std::shared_mutex> guard(_conn_mtx);
            _connections[fd]->clear();
        }
        _epoller->mod_fd(fd, EPOLLIN | EPOLLET | EPOLLONESHOT);
    }
    else {
        _close_sock(fd);
    }
}

// 设置socket为非阻塞
void Sub_Reactor_Pool::_set_nonblocking(int fd) {
    int flag = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flag | O_NONBLOCK);
}


void Sub_Reactor_Pool::_accecpt_new_sock() {
    std::unique_lock<std::mutex> unq_lock(_fd_queue_mtx);
    if(!_client_fds.empty()) {
        // 取一个任务
        auto cli_sock = _client_fds.front();
        _client_fds.pop();
        // 释放锁
        unq_lock.unlock();
        _epoller->add_fd(cli_sock, EPOLLIN | EPOLLET | EPOLLONESHOT);
        _set_nonblocking(cli_sock);
        {
            std::lock_guard<std::shared_mutex> guard(_conn_mtx);
            _connections[cli_sock] = std::make_shared<Http_Connection>(cli_sock);
        }
        // _connections.insert({cli_sock, std::make_shared<Http_Connection>(cli_sock)});

        std::cout<<"register successfully : socket "<<cli_sock<<"\n"<<std::endl;

        // 开启定时器
        if(_timeout >= 0) {
            _timer->push(cli_sock, _timeout, std::bind(&Sub_Reactor_Pool::_close_sock, this, cli_sock));
        }
    }
}