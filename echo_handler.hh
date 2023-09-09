#ifndef ECHO_HANDLER_H
#define ECHO_HANDLER_H

#include <vector>
#include <string>

// 每一个client socket对应一个handler，handler有自己的缓存
class Echo_Handler {
public:
    Echo_Handler(size_t size = 1024) : _rd_buf(size) {}
    std::vector<char> _rd_buf;
    std::string _wr_buf;

private:
    
};
#endif