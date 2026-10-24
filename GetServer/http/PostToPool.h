#ifndef GETSERVER_HTTP_POSTTOPOOL_H
#define GETSERVER_HTTP_POSTTOPOOL_H

#include "const.h"
#include "net/HttpConnection.h"

#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <exception>
#include <functional>
#include <memory>
#include <utility>

namespace http {
struct AtomicCounterGuard {
    std::atomic<std::size_t>& count;

    ~AtomicCounterGuard() {
        count.fetch_sub(1, std::memory_order_relaxed);
    }
};

inline void PostToPool(
        boost::asio::thread_pool& pool, std::atomic<std::size_t>& queued, std::size_t queue_limit, int queue_budget_ms,
        std::shared_ptr<HttpConnection> connection, std::function<void()> on_full, std::function<void()> on_failure,
        std::function<void()> on_abandon, std::function<void()> work
) {
    const std::size_t outstanding = queued.fetch_add(1, std::memory_order_relaxed);
    if (outstanding >= queue_limit) {
        queued.fetch_sub(1, std::memory_order_relaxed);
        on_full();
        return;
    }

    const auto enqueued_at = std::chrono::steady_clock::now();
    try {
        boost::asio::post(
                pool, [connection = std::move(connection), &queued, enqueued_at, queue_budget_ms,
                       on_full = std::move(on_full), on_failure = std::move(on_failure),
                       on_abandon = std::move(on_abandon), work = std::move(work)]() mutable {
                    AtomicCounterGuard guard{queued};
                    if (!connection || !connection->IsOpen()) {
                        if (on_abandon) {
                            on_abandon();
                        }
                        return;
                    }
                    if (std::chrono::steady_clock::now() - enqueued_at > std::chrono::milliseconds(queue_budget_ms)) {
                        on_full();
                        return;
                    }
                    try {
                        work();
                    } catch (const std::exception& e) {
                        LogThrottled("worker exception", e.what());
                        on_failure();
                    } catch (...) {
                        on_failure();
                    }
                }
        );
    } catch (...) {
        queued.fetch_sub(1, std::memory_order_relaxed);
        on_failure();
    }
}
}  // namespace http
#endif  // GETSERVER_HTTP_POSTTOPOOL_H
