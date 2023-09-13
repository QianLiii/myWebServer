#include "server.hh"

Server::Server(in_port_t port, size_t thread_num, int64_t timeout)
    : _port(port), _thread_num(thread_num), _timeout(timeout), _is_close(false),
    _epoller(new Epoller(MAX_EVENTS)),
    _pool(new Thread_Pool(thread_num)),
    _timer(new Timer())
{
    if(!_init_socket()) {
        throw Serv_Exception("init listen socket fail\n");
    }
}



// Server::~Server() {
//     // std::cout<<"Server destructed\n";
// }


void Server::start() {
    _loop();
}
// 初始化socket相关配置
bool Server::_init_socket() {
    sockaddr_in serv_addr{};
    if((_serv_sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        throw Serv_Exception("create socket fail\n");
    }
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    // 巨坑：一开始忘记调htons了，导致测试用的端口是9090，而一直连接不上，而且ps -a也看不到这个端口打开了。随后发现server监听了一个33315端口
    // 然后突然意识到自己完全忘记了htons的作用（主机序问题）
    serv_addr.sin_port = htons(_port);

    // 端口复用
    int optval = 1;
    if(setsockopt(_serv_sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        throw Serv_Exception("set option fail\n");
    }

    if(bind(_serv_sock, (const sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
        throw Serv_Exception("bind fail\n");
    }
    if(listen(_serv_sock, 8) == -1) {
        throw Serv_Exception("listen fail\n");
    }
    _set_nonblocking(_serv_sock);
    // _serv_sock只需要监听连接的创建和关闭事件
    if(!_epoller->add_fd(_serv_sock, EPOLLRDHUP | EPOLLIN)) {
        throw Serv_Exception("add fd fail\n");
    }
    return true;
}

// epoll_wait的主循环
void Server::_loop() {
    // 距下一次超时的时间，-1表示无超时时间
    int64_t next_timeout = -1;
    while(!_is_close) {
        // std::cout<<"looping...\n"<<std::endl;
        if(_timeout >= 0) {
            next_timeout = _timer->tick();
        }
        int num_of_events = _epoller->wait(next_timeout);
        if(num_of_events < 0) {
            throw Serv_Exception("epoll wait fail\n");
        }
        for(int i = 0;i<num_of_events;++i) {
            int fd = _epoller->get_sock_fd(i);
            uint32_t ev = _epoller->get_event(i);
            // 处理新连接
            if(fd == _serv_sock) {
                // std::cout<<"here comes a new connection\n"<<std::endl;
                _add_new_sock();
            }
            // 处理断开连接
            else if(ev & (EPOLLRDHUP | EPOLLERR | EPOLLHUP)) {
                _close_sock(fd);
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
    sockaddr_in addr{};
    socklen_t len{sizeof(addr)};
    int cli_sock = accept(_serv_sock, (sockaddr *)&addr, &len);
    if(cli_sock < 0) {
        throw Serv_Exception("accept new sock fail\n");
    }
    _epoller->add_fd(cli_sock, EPOLLIN | EPOLLET | EPOLLONESHOT);
    _set_nonblocking(cli_sock);
    {
        std::lock_guard<std::shared_mutex> guard(_conn_mtx);
        _connections[cli_sock] = std::make_shared<Http_Connection>(cli_sock);
    }
    // _connections.insert({cli_sock, std::make_shared<Http_Connection>(cli_sock)});

    // std::cout<<"register successfully : socket "<<cli_sock<<"\n"<<std::endl;

    // 开启定时器
    if(_timeout >= 0) {
        _timer->push(cli_sock, _timeout, std::bind(&Server::_close_sock, this, cli_sock));
    }
}

void Server::_close_sock(int fd) {
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
void Server::_read(int fd) {
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
    // std::cout<<"handle data from socket "<<fd<<"\n"<<std::endl;
    int ret;
    {
        std::shared_lock<std::shared_mutex> guard(_conn_mtx);
        ret = _connections[fd]->read();
    }
    if(ret == 0 || ret == ERR_READ_FAIL) {
        // 关闭连接
        // std::cout<<"read failed or received FIN.\n"<<std::endl;
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
            // std::cout<<"read over. now wait for write.\n"<<std::endl;
            _epoller->mod_fd(fd, EPOLLOUT | EPOLLET | EPOLLONESHOT);
        }
        // 否则注册一次读事件
        else {
            // std::cout<<"not fully read yet.\n"<<std::endl;
            _epoller->mod_fd(fd, EPOLLIN | EPOLLET | EPOLLONESHOT);
        }
    }
}

// 向client socket写
void Server::_write(int fd) {

    // std::cout<<"send data to socket "<<fd<<"\n"<<std::endl;
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
void Server::_set_nonblocking(int fd) {
    int flag = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flag | O_NONBLOCK);
}