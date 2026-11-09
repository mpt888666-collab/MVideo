//
// Created by mpt on 2026/10/15.
//

#ifndef GETSERVER_ASIOIOSERVICEPOOL_H
#define GETSERVER_ASIOIOSERVICEPOOL_H
#include "common/Singleton.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <vector>
#include <boost/asio.hpp>
#include <memory>

class AsioIOServicePool : public Singleton<AsioIOServicePool> {
    friend class Singleton<AsioIOServicePool>;

public:
    boost::asio::io_context& GetIOService();

    ~AsioIOServicePool();

private:
    AsioIOServicePool();

    void Stop();
    std::atomic<bool> _b_stop{false};
    std::atomic<int> _nextIOService{0};
    std::vector<std::thread> _threads;
    std::vector<boost::asio::io_context> _ioServices;
    std::vector<std::unique_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>> _works;
};

#endif  // GETSERVER_ASIOIOSERVICEPOOL_H
