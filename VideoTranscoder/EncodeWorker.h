//
// Created by mpt on 2026/11/2.
//

#ifndef VIDEOTRANSCODER_ENCODEWORKER_H
#define VIDEOTRANSCODER_ENCODEWORKER_H

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>

struct RequestInfo;
class EncodeWorkPool;

class EncodeWorker {
    constexpr static std::size_t kMaxQueueSize = 32;
    friend class EncodeMgr;

public:
    EncodeWorker();
    ~EncodeWorker();

    bool isClosed() const;
    void Stop();

private:
    void dealEncodeData();
    void startEncode(std::unique_ptr<RequestInfo> request_info);
    bool postEncodeQueue(std::unique_ptr<RequestInfo> request_info);

    std::size_t FreeSlot();

    std::shared_ptr<EncodeWorkPool> _encode_pool;
    std::queue<std::unique_ptr<RequestInfo>> _encode_queue;
    std::mutex _mtx;
    std::condition_variable _cv;
    std::atomic<bool> _is_shut_down{false};
    std::thread _deal_thread;
};

#endif  // VIDEOTRANSCODER_ENCODEWORKER_H