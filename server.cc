#include "server.hh"

Server::Server(in_port_t port, size_t thread_num, int64_t timeout)
    : _port(port), _thread_num(thread_num), _timeout(timeout), _is_close(false),
    _epoller(std::make_unique<Epoller>(MAX_EVENTS)),
    _pool(std::make_unique<Thread_Pool>(thread_num)),
    _timer(std::make_unique<Timer>())
{
    if(!_init_socket()) {
        throw std::exception();
    }
}

void Server::start() {
    std::cout<<clk::now().time_since_epoch().count()<<" server started"<<std::endl;
    _loop();
}
// 初始化socket相关配置
bool Server::_init_socket() {
    sockaddr_in serv_addr{};
    if((_serv_sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        throw std::exception();
    }
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    // 巨坑：一开始忘记调htons了，导致测试用的端口是9090，而一直连接不上，而且ps -a也看不到这个端口打开了。随后发现server监听了一个33315端口
    // 然后突然意识到自己完全忘记了htons的作用（主机序问题）
    serv_addr.sin_port = htons(_port);

    // 端口复用
    int optval = 1;
    if(setsockopt(_serv_sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        throw std::exception();
    }

    if(bind(_serv_sock, (const sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
        throw std::exception();
    }
    if(listen(_serv_sock, 8) == -1) {
        throw std::exception();
    }
    _set_nonblocking(_serv_sock);
    // _serv_sock只需要监听连接的创建和关闭事件
    if(!_epoller->add_fd(_serv_sock, EPOLLRDHUP | EPOLLIN)) {
        throw std::exception();
    }
    return true;
}

// epoll_wait的主循环
void Server::_loop() {
    // 距下一次超时的时间，-1表示无超时时间
    int64_t next_timeout = -1;
    while(!_is_close) {
        std::cout<<clk::now().time_since_epoch().count()<<" looping..."<<std::endl;
        if(_timeout >= 0) {
            next_timeout = _timer->tick();
        }
        int num_of_events = _epoller->wait(next_timeout);
        if(num_of_events < 0) {
            throw std::exception();
        }
        for(int i = 0;i<num_of_events;++i) {
            int fd = _epoller->get_sock_fd(i);
            uint32_t ev = _epoller->get_event(i);
            // 处理新连接
            if(fd == _serv_sock) {
                _add_new_sock();
            }
            // 处理断开连接
            else if(ev & (EPOLLRDHUP | EPOLLERR | EPOLLHUP)) {
                _close_sock(fd);
                std::cout<<clk::now().time_since_epoch().count()<<" connection close\n";
            }
            // 分发读事件
            else if(ev & EPOLLIN) {
                // 这里应该进一步封装
                _timer->update(fd, _timeout);
                // bind成员函数要传入this
                _pool->push_task(std::bind(&Server::_read, this, fd));
            }
            // 分发写事件
            else if(ev & EPOLLOUT) {
                _timer->update(fd, _timeout);
                _pool->push_task(std::bind(&Server::_write, this, fd));
            }
        }
    }
}

// 向_epoller中添加新的socket
void Server::_add_new_sock() {
    std::cout<<clk::now().time_since_epoch().count()<<" new client comes\n";
    sockaddr_in addr{};
    socklen_t len{sizeof(addr)};
    int cli_sock = accept(_serv_sock, (sockaddr *)&addr, &len);
    if(cli_sock < 0) {
        throw std::exception();
    }
    _epoller->add_fd(cli_sock, EPOLLIN | EPOLLET | EPOLLONESHOT);
    _set_nonblocking(cli_sock);
    _handlers[cli_sock] = std::make_shared<Echo_Handler>();
    // 开启定时器
    if(_timeout >= 0) {
        _timer->push(cli_sock, _timeout, std::bind(&Server::_close_sock, this, cli_sock));
    }
}

void Server::_close_sock(int fd) {
    _epoller->del_fd(fd);
    _handlers.erase(fd);
    // 要主动关闭对应的定时器（不执行回调函数）
    _timer->erase(fd);
    close(fd);
}

// 从client socket读，非阻塞IO，边沿触发
void Server::_read(int fd) {
    std::cout<<clk::now().time_since_epoch().count()<<" handling reading...\n";
    auto & handler = _handlers[fd];
    int total_bytes = 0;
    while(true) {
        int len = read(fd, (void *)&(handler->_rd_buf[0]), 4);
        if(len == 0) {
            // 关闭连接
            std::cout << "connection close because EOF\n";
            _close_sock(fd);
            return;
        }
        if(len < 0 ) {
            // 当前没有可读数据
            if(errno == EAGAIN) {
                break;
            }
            else {
                throw std::exception();
            }
        }
        else {
            total_bytes += len;
            // 对读到的数据处理
            handler->_wr_buf.append(&(handler->_rd_buf[0]), len);
        }
    }
    // 完成了读任务，注册一次写事件
    if(total_bytes > 0) {
        _epoller->mod_fd(fd, EPOLLOUT | EPOLLET | EPOLLONESHOT);
    }
    // 否则注册一次读事件
    else {
        _epoller->mod_fd(fd, EPOLLIN | EPOLLET | EPOLLONESHOT);
    }
}

// 向client socket写，一次写完，没有特殊的处理
void Server::_write(int fd) {
    std::cout<<clk::now().time_since_epoch().count()<<" handling writing...\n";
    auto &buf = _handlers[fd]->_wr_buf;
    write(fd, &(buf[0]), buf.size());

    // 写完后注册读事件
    _epoller->mod_fd(fd, EPOLLIN | EPOLLET | EPOLLONESHOT);
}

// 设置socket为非阻塞
void Server::_set_nonblocking(int fd) {
    int flag = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flag | O_NONBLOCK);
}