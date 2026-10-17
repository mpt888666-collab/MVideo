//
// Created by mpt on 2026/10/15.
//

#include "net/AsioIOServicePool.h"

AsioIOServicePool::AsioIOServicePool(const std::size_t size) : _b_stop(false), _nextIOService(0),
                                                         _ioServices(size), _works(size) {
    for (std::size_t i = 0; i < size; ++i) {
        _works[i] = std::make_unique<boost::asio::executor_work_guard<boost::asio::io_context::executor_type> >(
            _ioServices[i].get_executor());

        _threads.emplace_back([this, i]() {
            _ioServices[i].run();
        });
    }
}

AsioIOServicePool::~AsioIOServicePool() {
    Stop();
    std::cout << "AsioIOServicePool destruct" << std::endl;
}

boost::asio::io_context& AsioIOServicePool::GetIOService() {
    auto &io_service = _ioServices[(_nextIOService++) % _ioServices.size()];
    return io_service;
}

void AsioIOServicePool::Stop() {
    for (auto & work: _works) {
        work.reset();
    }

    for (auto& io_service: _ioServices) {
        io_service.stop();
    }

    for (auto & thread: _threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}
