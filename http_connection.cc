#include "http_connection.hh"
#include <iostream>

#define TMP_BUFFER_SIZE 64

Http_Connection::Http_Connection(int fd) : _no_keep_alive(false), _fd(fd), _read_buffer() {

}

int Http_Connection::read() {
    char tmp_buf[TMP_BUFFER_SIZE];
    while(true) {
        int len = ::read(_fd, &tmp_buf, TMP_BUFFER_SIZE);
        if(len == 0) {
            // 关闭连接
            return 0;
        }
        if(len < 0 ) {
            // 当前没有可读数据
            if(errno == EAGAIN) {
                return -1;
            }
            else {
                // 一些错误的处理
                return ERR_READ_FAIL;
            }
        }
        else {
            _read_buffer.append(tmp_buf);
        }
    }
}

int Http_Connection::write() {
    int ret = _parse_request_line();
    if(ret != ERR_SUCCESS) {
        // 这里发送相应的错误响应
        _handle_error(ret);
        return ret;
    }
    ret = _parse_header();
    if(ret != ERR_SUCCESS) {
        // 这里发送相应的错误响应
        _handle_error(ret);
        return ret;
    }
    _send_response("200", "OK", "echo request body: \n"+_read_buffer.substr(_body_start_pos));
    return ERR_SUCCESS;
}

int Http_Connection::_parse_request_line() {
    size_t pos1 = std::string::npos, pos2 = pos1;
    pos1 = _read_buffer.find(' ');
    if(pos1 == std::string::npos) {
        std::cout<<"position 1\n";
        return ERR_BAD_REQUEST;
    }
    _method = _read_buffer.substr(0, pos1);
    // 只支持GET
    if(_method != "GET") {
        return ERR_NOT_IMPLEMENTED;
    }
    ++pos1;
    pos2 = _read_buffer.find(' ', pos1);
    if(pos2 == std::string::npos) {
        std::cout<<"position 2\n";
        return ERR_BAD_REQUEST;
    }
    ++pos1;
    _uri = _read_buffer.substr(pos1, pos2 - pos1);
    if(_uri.empty()) {
        _uri = ".";
    }
    // uri还需要一些额外的处理
    // ...
    ++pos2;
    _header_start_pos = _read_buffer.find("\r\n");
    if(_header_start_pos == std::string::npos) {
        std::cout<<"position 3\n";
        return ERR_BAD_REQUEST;
    }
    _version = _read_buffer.substr(pos2, _header_start_pos - pos2);
    if(_version != "HTTP/1.1") {
        return ERR_HTTP_VERSION_NOT_SUPPORTED;
    }
    _header_start_pos += 2;
    return ERR_SUCCESS;
}

int Http_Connection::_parse_header() {
    size_t pos1= _header_start_pos, line_end;
    for(;(line_end = _read_buffer.find("\r\n", pos1)) != std::string::npos;
    pos1 = line_end + 2) {
        // 空行
        if(pos1 == line_end) {
            break;
        }
        // 严重错误：string_view只是保存字符串的指针，substr生成的临时量被销毁后，string_view不再有意义
        // std::string_view line = _read_buffer.substr(pos1, pos2 - pos1);

        size_t line_begin = pos1;
        pos1 = _read_buffer.find(": ", line_begin);
        if(pos1 == std::string::npos) {
            std::cout<<"position 4\n";
            return ERR_BAD_REQUEST;
        }

        // 把header的key转小写
        std::transform(_read_buffer.begin()+line_begin, _read_buffer.begin()+pos1, _read_buffer.begin()+line_begin, ::tolower);

        std::string_view key = _sub_view(line_begin, pos1);

        _header[key] = _sub_view(pos1+2, line_end-pos1-2);
        auto it = _header.find("connection");
        if(it != _header.end()) {
            std::string val(it->second);
            std::transform(val.begin(), val.end(),val.begin(), ::tolower);
            if(val != "keep-alive") {
                _no_keep_alive = true;
            }
        }
    }
    if(pos1 >= _read_buffer.size() || _read_buffer.substr(pos1, 2) != "\r\n") {
        std::cout<<"position 5\n";
        return ERR_BAD_REQUEST;
    }
    _body_start_pos = pos1 + 2;
    return ERR_SUCCESS;
}

int Http_Connection::_send_response(std::string_view response_code, std::string_view response_msg, std::string_view body) {
    std::ostringstream ss;
    ss<<"HTTP/1.1 "<<response_code<<" "<<response_msg<<"\r\n"
    <<"Connection: "<<(_no_keep_alive?"Close":"Keep-Alive")<<"\r\n"
    <<"Content-Length: "<<body.size()<<"\r\n"
    <<"Content-Type: text/html;charset=utf-8\r\n\r\n"
    <<body;
    std::string &&response = ss.str();
    if(::write(_fd, &(response[0]), response.size()) != response.size()) {
        return ERR_SEND_FAIL;
    }
    return ERR_SUCCESS;
}


void Http_Connection::_handle_error(int ERR) {
    std::string code, msg;
    if(ERR == ERR_HTTP_VERSION_NOT_SUPPORTED) {
        code = "505";
        msg = "HTTP Version Not Supported";
    }
    else if(ERR == ERR_BAD_REQUEST) {
        code = "400";
        msg = "Bad Request";
    }
    else if(ERR == ERR_NOT_IMPLEMENTED) {
        code = "501";
        msg = "Not Implemented";
    }
    else if(ERR == ERR_NOT_FOUND) {
        code = "404";
        msg = "Not Found";
    }

    _send_response(code, msg, code+" "+msg);
}

// 获取read buffer的某段string_view
std::string_view Http_Connection::_sub_view(size_t begin, size_t end) {
    std::string_view sv = _read_buffer;
    sv.remove_prefix(begin);
    sv.remove_suffix(end);
    return sv;
}