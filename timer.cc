#include "timer.hh"

void Timer::set(int fd, size_t timeout, std::function<void()> &&callback) {
    if(_mapping.count(fd) > 0) {
        update(fd, timeout);
        _heap[_mapping[fd]]._callback = std::forward<std::function<void()>>(callback);
    }
    else {

    }
}

void Timer::update(int fd, size_t timeout) {

}

int Timer::tick() {
    _tick;
    if(_heap.empty()) {
        return -1;
    }
    else {
        auto time_diff = std::chrono::duration_cast<std::chrono::milliseconds>(_heap.front()._expire_time - clk::now()).count();
        return time_diff > 0 ? time_diff : 0;
    }
}










// 清理超时的定时器，延迟到一次上层调用tick()再处理
void Timer::_tick() {
    while(!_heap.empty()) {
        auto oldest = _heap.front();
        // 如果最早的定时器还没有超时，后面的也不会超时
        if(std::chrono::duration_cast<std::chrono::milliseconds>(oldest._expire_time - clk::now()).count() > 0) {
            return;
        }
        oldest._callback();
        // 堆顶出列

    }
}