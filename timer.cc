#include <iostream>

#include "timer.hh"

// 插入新定时器
void Timer::push(int fd, int64_t timeout, std::function<void()> &&callback) {
    if(_mapping.count(fd) > 0) {
        update(fd, timeout);
        _heap[_mapping[fd]]._callback = std::forward<std::function<void()>>(callback);
    }
    else {
        size_t idx = _heap.size();
        _mapping[fd] = idx;
        _heap.push_back({fd, clk::now() + std::chrono::milliseconds(timeout), std::forward<std::function<void()>>(callback)});
        _swim(idx);
    }
}

// 更新时间
void Timer::update(int fd, int64_t timeout) {
    if(_mapping.count(fd) == 0) {
        return;
    }
    int idx = _mapping[fd];
    _heap[idx]._expire_time = clk::now() + std::chrono::milliseconds(timeout);
    _sink(idx);
}

void Timer::erase(int fd) {
    if(_mapping.count(fd) == 0) {
        return;
    }
    _erase(_mapping[fd]);
}

// 上浮
void Timer::_swim(size_t i) {
    int parent = (i - 1) / 2;
    while(parent >= 0) {
        if(_heap[i] < _heap[parent]) {
            _swap(i, parent);
            parent = (parent - 1) / 2;
            i = parent;
        }
    }
}

// 下潜
void Timer::_sink(size_t i) {
    while(2 * i + 2 <= _heap.size()) {
        size_t child = 2 * i + 1;
        // 有右儿子且更小
        if(2 * i + 2 < _heap.size() && _heap[2*i+2] < _heap[2*i+1]) {
           ++child;
        }
        // 根更小无需更新
        if(_heap[i] < _heap[child]) {
            return;
        }
        else {
            _swap(i, child);
            i = child;
        }
    }
}

// 交换两个Timer_Node
void Timer::_swap(size_t lhs_idx, size_t rhs_idx) {
    int lhs_fd = _heap[lhs_idx]._fd, rhs_fd = _heap[rhs_idx]._fd;
    std::swap(_heap[lhs_idx], _heap[rhs_idx]);
    _mapping[lhs_fd] = rhs_idx;
    _mapping[rhs_fd] = lhs_idx;
}

// 删除随机元素
void Timer::_erase(size_t i) {
    if(_heap.empty()) {
        return;
    }
    int fd = _heap[i]._fd;
    _swap(i, _heap.size()-1);
    _heap.pop_back();
    _mapping.erase(fd);
    _sink(i);
    _swim(i);
}



int64_t Timer::tick() {
    _tick();
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
        std::cout<<"client socket "<<oldest._fd<<" closed because expired.\n";
        oldest._callback();
        _erase(0);
    }
}