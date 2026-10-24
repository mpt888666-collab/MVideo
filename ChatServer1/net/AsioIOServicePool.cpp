//
// Created by mpt on 2026/10/15.
//

#include "net/AsioIOServicePool.h"

#include <iostream>

#include "config/ConfigMgr.h"

namespace {

std::size_t ResolveIoThreadCount() {
    int value = 0;
    try {
        auto configured = ConfigMgr::Inst()["SelfServer"]["IoThreads"];
        if (configured.empty()) {
            configured = ConfigMgr::Inst()["GetServer"]["IoThreads"];
        }
        if (!configured.empty()) {
            value = std::stoi(configured);
        }
    } catch (...) {
    }
    if (value <= 0) {
        const unsigned int hardware = std::thread::hardware_concurrency();
        value = hardware > 0 ? static_cast<int>(hardware / 2) : 8;
        if (value < 8)
            value = 8;
        if (value > 16)
            value = 16;
    }
    if (value < 1)
        value = 1;
    if (value > 32)
        value = 32;
    return static_cast<std::size_t>(value);
}

}  // namespace

AsioIOServicePool::AsioIOServicePool()
    : _b_stop(false),
      _nextIOService(0),
      _ioServices(ResolveIoThreadCount()),
      _works(_ioServices.size()) {
    const std::size_t size = _ioServices.size();
    for (std::size_t i = 0; i < size; ++i) {
        _works[i] = std::make_unique<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>(
                _ioServices[i].get_executor()
        );

        _threads.emplace_back([this, i]() { _ioServices[i].run(); });
    }
}

AsioIOServicePool::~AsioIOServicePool() {
    Stop();
    std::cout << "AsioIOServicePool destruct" << std::endl;
}

boost::asio::io_context& AsioIOServicePool::GetIOService() {
    auto& io_service = _ioServices[(_nextIOService++) % _ioServices.size()];
    return io_service;
}

void AsioIOServicePool::Stop() {
    for (auto& work : _works) {
        work.reset();
    }

    for (auto& io_service : _ioServices) {
        io_service.stop();
    }

    for (auto& thread : _threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}
