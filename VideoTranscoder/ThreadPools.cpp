//
// Created by mpt on 2026/11/2.
//

#include "ThreadPools.h"

#include <iostream>
#include <thread>

namespace {
    std::size_t ComputeWorkerCount() {
        const unsigned hardware = std::thread::hardware_concurrency();
        std::size_t count = hardware == 0 ? 2 : hardware / 4;
        if (count < 1) {
            count = 1;
        }
        if (count > 4) {
            count = 4;
        }
        return count;
    }
}

EncodeWorkPool::EncodeWorkPool()
    : _max_concurrent(ComputeWorkerCount()), _worker(boost::asio::thread_pool(_max_concurrent)) {
    std::cout << "[EncodeWorkPool] threads = " << _max_concurrent << std::endl;
}

boost::asio::thread_pool& EncodeWorkPool::GetPool() {
    return _worker;
}

bool EncodeWorkPool::TryAcquire() {
    if (_closed.load(std::memory_order_acquire)) {
        return false;
    }

    std::size_t current = _active.load(std::memory_order_relaxed);
    while (current < _max_concurrent) {
        if (_active.compare_exchange_weak(current, current + 1, std::memory_order_acq_rel,
                                          std::memory_order_relaxed)) {
            return true;
        }
    }
    return false;
}

void EncodeWorkPool::Release() {
    std::size_t current = _active.load(std::memory_order_relaxed);
    while (current > 0 &&
           !_active.compare_exchange_weak(current, current - 1, std::memory_order_acq_rel,
                                          std::memory_order_relaxed)) {
    }
}

void EncodeWorkPool::Stop() {
    _closed.store(true, std::memory_order_release);
    _worker.join();
}

EncodeWorkPool::~EncodeWorkPool() {
    Stop();
}