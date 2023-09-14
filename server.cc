#include "server.hh"

Server::Server(in_port_t port, size_t thread_num, int64_t timeout)
    : _port(port), _thread_num(thread_num), _timeout(timeout), _is_close(false),
    _epoller(new Epoller(MAX_EVENTS)),
    _sub_reactor_pool(new Sub_Reactor_Pool(thread_num, timeout)),
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
        std::cout<<"main looping...\n"<<std::endl;
        if(_timeout >= 0) {
            next_timeout = _timer->tick();
        }
        int num_of_events = _epoller->wait(next_timeout);
        if(num_of_events < 0) {
            throw Serv_Exception("epoll wait fail\n");
        }
        for(int i = 0;i<num_of_events;++i) {
            int fd = _epoller->get_sock_fd(i);
            // 处理新连接
            if(fd == _serv_sock) {
                _add_new_sock();
            }
        }
    }
}

// 向_epoller中添加新的socket
void Server::_add_new_sock() {
    std::cout<<"hand new sock to sub reactor\n"<<std::endl;
    sockaddr_in addr{};
    socklen_t len{sizeof(addr)};
    int cli_sock = accept(_serv_sock, (sockaddr *)&addr, &len);
    if(cli_sock < 0) {
        throw Serv_Exception("accept new sock fail\n");
    }
    _sub_reactor_pool->push_client_fd(cli_sock);
}



// 设置socket为非阻塞
void Server::_set_nonblocking(int fd) {
    int flag = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flag | O_NONBLOCK);
}