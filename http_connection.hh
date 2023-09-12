#ifndef HTTP_CONNECTION_H
#define HTTP_CONNECTION_H

#include <unistd.h>
#include <errno.h>
#include <string>
#include <string_view>
#include <map>
#include <algorithm>
#include <sstream>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>

#include "serv_exception.hh"

enum ERROR_TYPE {
    ERR_SUCCESS = 0,
    ERR_CONNECTION_CLOSED,
    ERR_READ_FAIL,
    ERR_INTERNAL_SERVER_ERROR,
    ERR_HTTP_VERSION_NOT_SUPPORTED,
    ERR_BAD_REQUEST,
    ERR_NOT_IMPLEMENTED,
    ERR_NOT_FOUND,
    ERR_SEND_FAIL
};

class Http_Connection {



public:
    Http_Connection(int fd);
    int read();
    int write();

    bool read_ready() const {return !_read_buffer.empty();}
    bool need_keep_alive() const {return !_no_keep_alive;}
    void clear() {
        _no_keep_alive = false;
        _fd = -1;
        _read_buffer.clear();
        _header.clear();
    }



    std::string __test_get_buffer() {return _read_buffer;}

private:
    int _parse_request_line();
    int _parse_header();

    int _send_response(std::string_view response_code, std::string_view response_msg, std::string_view body);
    void _handle_error(int ERR);

    std::string_view _sub_view(size_t begin, size_t end);

    std::string _file_to_str(std::string_view path);

private:
    bool _no_keep_alive;
    int _fd;
    std::string _read_buffer;
    std::string _method;
    std::string _uri;
    std::string _version;
    std::map<std::string_view, std::string_view> _header;

    size_t _header_start_pos;   // 用来记录请求头开始位置
    size_t _body_start_pos;
};

#endif