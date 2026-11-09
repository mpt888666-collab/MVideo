//
// Created by mpt on 2026/11/2.
//

#include "EncodeWorker.h"

#include <boost/asio.hpp>

#include <algorithm>
#include <functional>
#include <iostream>

#include "FfmpegJob.h"
#include "ThreadPools.h"
#include "const.h"

namespace {
    void NotifyFinished(RequestInfo& info) {
        if (info.on_finished) {
            auto fn = std::move(info.on_finished);
            info.on_finished = nullptr;
            fn();
        }
    }
}

EncodeWorker::EncodeWorker() : _encode_pool(EncodeWorkPool::GetInstance()) {
    _deal_thread = std::thread([this] { dealEncodeData(); });
}

EncodeWorker::~EncodeWorker() {
    Stop();
}

bool EncodeWorker::postEncodeQueue(std::unique_ptr<RequestInfo> request_info) {
    std::unique_lock<std::mutex> lock(_mtx);
    if (_is_shut_down.load() || _encode_queue.size() >= kMaxQueueSize) {
        return false;
    }
    _encode_queue.push(std::move(request_info));
    _cv.notify_one();
    return true;
}

std::size_t EncodeWorker::FreeSlot() {
    std::lock_guard<std::mutex> lock(_mtx);
    if (_is_shut_down.load() || !_encode_pool) {
        return 0;
    }

    const std::size_t queue_free =
            _encode_queue.size() >= kMaxQueueSize ? 0 : kMaxQueueSize - _encode_queue.size();
    const std::size_t max_concurrent = _encode_pool->MaxConcurrent();
    const std::size_t active = _encode_pool->ActiveCount();
    const std::size_t slot_free = active >= max_concurrent ? 0 : max_concurrent - active;

    return std::min(queue_free, slot_free);
}

bool EncodeWorker::isClosed() const {
    return _is_shut_down.load();
}

void EncodeWorker::Stop() {
    {
        std::unique_lock<std::mutex> lock(_mtx);
        if (_is_shut_down.exchange(true)) {
            return;
        }
        _cv.notify_all();
        while (!_encode_queue.empty()) {
            _encode_queue.pop();
        }
    }

    if (_deal_thread.joinable()) {
        _deal_thread.join();
    }
    if (_encode_pool) {
        _encode_pool->Stop();
    }
}

void EncodeWorker::dealEncodeData() {
    for (;;) {
        std::unique_ptr<RequestInfo> request_info;
        {
            std::unique_lock<std::mutex> lock(_mtx);
            _cv.wait(lock, [this] { return !_encode_queue.empty() || _is_shut_down.load(); });
            if (_is_shut_down.load()) {
                return;
            }
            request_info = std::move(_encode_queue.front());
            _encode_queue.pop();
        }
        if (request_info) {
            startEncode(std::move(request_info));
        }
    }
}

void EncodeWorker::startEncode(std::unique_ptr<RequestInfo> request_info) {
    auto pool = _encode_pool;
    if (!pool || !request_info) {
        return;
    }

    if (!pool->TryAcquire()) {
        std::cerr << "[EncodeWorker] pool busy, defer video_id = " << request_info->_video_id
                  << std::endl;
        NotifyFinished(*request_info);
        return;
    }

    try {
        boost::asio::post(pool->GetPool(), [info = std::move(request_info), pool]() mutable {
            struct SlotRelease {
                EncodeWorkPool* pool;
                ~SlotRelease() { pool->Release(); }
            } release{pool.get()};

            struct FinishNotify {
                std::function<void()> fn;
                ~FinishNotify() {
                    if (fn) {
                        fn();
                    }
                }
            } notify;

            if (!info) {
                return;
            }
            notify.fn = std::move(info->on_finished);

            try {
                FfmpegJob job(info->_video_id);
                job.Run();
            } catch (const std::exception& e) {
                std::cerr << "[EncodeWorker] job " << info->_video_id
                          << " threw: " << e.what() << std::endl;
            } catch (...) {
                std::cerr << "[EncodeWorker] job " << info->_video_id
                          << " threw unknown exception" << std::endl;
            }
        });
    } catch (const std::exception& e) {
        std::cerr << "[EncodeWorker] post failed: " << e.what() << std::endl;
        pool->Release();
        NotifyFinished(*request_info);
    }
}