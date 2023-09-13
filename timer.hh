#ifndef TIMER_H
#define TIMER_H

#include <memory>
#include <chrono>
#include <functional>
#include <deque>
#include <unordered_map>
#include <mutex>

using clk = std::chrono::high_resolution_clock;

// 可随机访问的小根堆实现的定时器系统
class Timer {
    // 单个定时器
    struct Timer_Node {
        Timer_Node(int fd, clk::time_point exp_time, std::function<void()>&& callback) :
        _fd(fd), _expire_time(exp_time), _callback(callback) {}

        int _fd;
        clk::time_point _expire_time;
        std::function<void()> _callback;
        bool operator<(const Timer_Node& rhs) const {
            return this->_expire_time < rhs._expire_time;
        }
    };
public:
    // 为fd创建一个在timeout后超时的定时器，执行callback
    void push(int fd, int64_t timeout, std::function<void()> &&callback);
    // 更新fd的定时器时间
    void update(int fd, int64_t timeout);
    // 删除对应的定时器
    void erase(int fd);
    // 返回最早的定时器距现在的时间；如果超时了，返回0；如果没有定时器开启，返回-1（这表明没有任何连接建立了，epoller可以阻塞等待）
    int64_t tick();

private:
    std::deque<std::shared_ptr<Timer_Node>> _heap;   // 堆的存储结构
    std::unordered_map<int, size_t> _mapping;   // 从fd到_heap下标的映射
    std::mutex _mtx;

private:
    void _swim(size_t i);
    void _sink(size_t i);
    void _swap(size_t lhs_idx, size_t rhs_idx);
    void _erase(size_t i);
    void _tick();
};

#endif