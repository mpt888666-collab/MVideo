//
// Created by mpt on 2026/11/2.
//

#include "EncodePoller.h"

#include <exception>
#include <iostream>
#include <utility>
#include <vector>

#include "EncodeMgr.h"
#include "MySqlMgr.h"
#include "const.h"

EncodePoller::EncodePoller(boost::asio::io_context& ioc)
    : _ioc(ioc), _getEncodeVideoTimer(ioc), _in_flight(std::make_shared<InFlight>()) {}

EncodePoller::~EncodePoller() {
    Stop();
}

bool EncodePoller::InFlight::Add(unsigned long long video_id) {
    const auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(mtx);
    for (auto it = ids.begin(); it != ids.end();) {
        if (now - it->second > kInFlightTtl) {
            it = ids.erase(it);
        } else {
            ++it;
        }
    }
    return ids.try_emplace(video_id, now).second;
}

void EncodePoller::InFlight::Remove(unsigned long long video_id) {
    std::lock_guard<std::mutex> lock(mtx);
    ids.erase(video_id);
}

std::size_t EncodePoller::InFlight::Count() {
    std::lock_guard<std::mutex> lock(mtx);
    return ids.size();
}

void EncodePoller::Run() {
    if (_stopped.load(std::memory_order_acquire)) {
        return;
    }
    startEncodeTimer();
}

void EncodePoller::startEncodeTimer() {
    if (_stopped.load(std::memory_order_acquire)) {
        return;
    }

    _getEncodeVideoTimer.expires_after(std::chrono::seconds(kPollIntervalSec));
    _getEncodeVideoTimer.async_wait([this](const boost::system::error_code& ec) {
        if (ec) {
            return;
        }
        try {
            pollOnce();
        } catch (const std::exception& e) {
            std::cerr << "[EncodePoller] poll failed: " << e.what() << std::endl;
        }
        startEncodeTimer();
    });
}

void EncodePoller::pollOnce() {
    const auto encode_mgr = EncodeMgr::GetInstance();
    if (!encode_mgr) {
        return;
    }

    const std::size_t free_slot = encode_mgr->FreeSlot();
    if (free_slot == 0) {
        return;
    }

    const auto video_ids = MySqlMgr::GetInstance()->PickPendingVideoIds(free_slot);
    if (video_ids.empty()) {
        return;
    }

    std::size_t posted = 0;
    for (const auto video_id : video_ids) {
        if (!_in_flight->Add(video_id)) {
            continue;
        }

        auto request_info = std::make_unique<RequestInfo>();
        request_info->_video_id = video_id;
        request_info->on_finished = [flight = _in_flight, video_id] { flight->Remove(video_id); };

        if (!encode_mgr->PostEncodeQueue(std::move(request_info))) {
            _in_flight->Remove(video_id);
            break;
        }
        ++posted;
    }

    std::cout << "[EncodePoller] picked " << video_ids.size() << ", queued " << posted
              << ", in flight " << _in_flight->Count() << std::endl;
}

void EncodePoller::Stop() {
    if (_stopped.exchange(true, std::memory_order_acq_rel)) {
        return;
    }
    try {
        _getEncodeVideoTimer.cancel();
    } catch (const std::exception& e) {
        std::cerr << "[EncodePoller] cancel timer failed: " << e.what() << std::endl;
    }
}