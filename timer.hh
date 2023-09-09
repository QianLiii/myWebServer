#ifndef TIMER_H
#define TIMER_H

#include <chrono>
#include <functional>
#include <deque>
#include <unordered_map>

using clk = std::chrono::high_resolution_clock;



// 可随机访问的小根堆实现的定时器系统
class Timer {
    // 单个定时器
    struct Timer_Node {
        int _fd;
        clk::time_point _expire_time;
        std::function<void()> _callback;
    };
public:
    // 为fd创建一个在timeout后超时的定时器，执行callback
    void set(int fd, size_t timeout, std::function<void()> &&callback);
    // 更新fd的定时器时间
    void update(int fd, size_t timeout);
    // 返回最早的定时器距现在的时间；如果超时了，返回0；如果没有定时器开启，返回-1（这表明没有任何连接建立了，epoller可以阻塞等待）
    int tick();

private:
    std::deque<Timer_Node> _heap;   // 堆的存储结构
    std::unordered_map<int, size_t> _mapping;   // 从fd到_heap下标的映射

private:

    void _tick();

    
};


#endif