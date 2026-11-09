//
// Created by mpt on 2026/11/2.
//

#ifndef VIDEOTRANSCODER_GETENCODEVIDEO_H
#define VIDEOTRANSCODER_GETENCODEVIDEO_H

#include <atomic>
#include <chrono>
#include <cstddef>
#include <memory>
#include <mutex>
#include <unordered_map>

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>

class EncodePoller {
public:
    explicit EncodePoller(boost::asio::io_context& ioc);
    ~EncodePoller();

    EncodePoller(const EncodePoller&) = delete;
    EncodePoller& operator=(const EncodePoller&) = delete;

    void Run();
    void Stop();

private:
    static constexpr int kPollIntervalSec = 10;
    static constexpr std::chrono::minutes kInFlightTtl{10};

    struct InFlight {
        std::mutex mtx;
        std::unordered_map<unsigned long long, std::chrono::steady_clock::time_point> ids;

        bool Add(unsigned long long video_id);
        void Remove(unsigned long long video_id);
        std::size_t Count();
    };

    void startEncodeTimer();
    void pollOnce();

    boost::asio::io_context& _ioc;
    boost::asio::steady_timer _getEncodeVideoTimer;
    std::shared_ptr<InFlight> _in_flight;
    std::atomic<bool> _stopped{false};
};

#endif  // VIDEOTRANSCODER_GETENCODEVIDEO_H