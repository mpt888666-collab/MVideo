//
// Created by mpt on 2026/11/2.
//

#ifndef VIDEOTRANSCODER_THREADPOOLS_H
#define VIDEOTRANSCODER_THREADPOOLS_H

#include <atomic>
#include <cstddef>

#include <boost/asio.hpp>

#include "Singleton.h"

class EncodeWorkPool : public Singleton<EncodeWorkPool> {
    friend class Singleton<EncodeWorkPool>;

public:
    boost::asio::thread_pool& GetPool();

    bool TryAcquire();
    void Release();

    std::size_t ActiveCount() const { return _active.load(std::memory_order_relaxed); }
    std::size_t MaxConcurrent() const { return _max_concurrent; }

    void Stop();
    ~EncodeWorkPool();

private:
    EncodeWorkPool();

    std::size_t _max_concurrent{0};
    boost::asio::thread_pool _worker;
    std::atomic<std::size_t> _active{0};
    std::atomic<bool> _closed{false};
};

#endif  // VIDEOTRANSCODER_THREADPOOLS_H