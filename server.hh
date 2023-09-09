#ifndef SERVER_H
#define WERVER_H

#include <sys/socket.h>
#include <arpa/inet.h>
#include <memory>
#include <errno.h>
#include <map>
#include <iostream>

#include "epoller.hh"
#include "echo_handler.hh"

#define MAX_EVENTS 1024

class Server {
public:
    Server(in_port_t port, size_t thread_num, size_t timeout);
    void start();
private:
    in_port_t _port;
    size_t _thread_num;
    size_t _timeout;
    bool _is_close;
    int _serv_sock;
    std::unique_ptr<Epoller> _epoller;
    std::map<int, std::shared_ptr<Echo_Handler>> _handlers;
    // threadpool

private:
    bool _init_socket();
    void _set_nonblocking(int fd);
    void _add_new_sock();
    void _read(int fd);
    void _write(int fd);
    void _loop();
};
#endif