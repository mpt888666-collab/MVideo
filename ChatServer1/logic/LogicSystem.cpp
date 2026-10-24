//
// Logic thread pool and message dispatch.
//

#include "logic/LogicSystem.h"

#include <cstddef>
#include <exception>
#include <functional>
#include <iostream>
#include <string>
#include <utility>

#include "common/ThreadPool.h"
#include "const.h"
#include "logic/LogicNode.h"
#include "net/CSession.h"
#include "net/MsgNode.h"
#include "service/LoginService.h"

namespace {

constexpr std::size_t kDefaultLogicWorkers = 4;
constexpr std::size_t kDefaultLogicQueueLimit = 1024;

std::size_t ResolveLogicWorkerCount() {
    return bounded_pool::ResolveCount("ChatServer", "LogicWorkers", kDefaultLogicWorkers, 1, 16);
}

std::size_t ResolveLogicQueueLimit() {
    return bounded_pool::ResolveCount("ChatServer", "LogicQueueLimit", kDefaultLogicQueueLimit, 32, 8192);
}

}  // namespace

LogicSystem::LogicSystem() {
    const auto count = ResolveLogicWorkerCount();
    _works.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        _works.push_back(std::make_unique<LogicWork>());
    }
}

bool LogicSystem::PostLogicQueue(std::shared_ptr<LogicNode> logic_node) {
    std::lock_guard<std::mutex> lock(_mtx);
    if (_b_stop_push.load(std::memory_order_acquire) || !logic_node || !logic_node->_session) {
        return false;
    }

    const int shard = ShardForSessionLocked(logic_node->_session->GetSessionId());
    if (shard < 0) {
        return false;
    }
    return _works[static_cast<std::size_t>(shard)]->PostLogicNode(std::move(logic_node));
}

bool LogicSystem::PostContinuation(const std::string& session_id, std::function<void()> task) {
    std::lock_guard<std::mutex> lock(_mtx);
    if (_b_stop_push.load(std::memory_order_acquire) || !task) {
        return false;
    }

    const int shard = ShardForSessionLocked(session_id);
    if (shard < 0) {
        return false;
    }
    return _works[static_cast<std::size_t>(shard)]->PostTask(std::move(task));
}

int LogicSystem::ShardForSession(const std::string& session_id) const {
    std::lock_guard<std::mutex> lock(_mtx);
    return ShardForSessionLocked(session_id);
}

std::size_t LogicSystem::WorkerCount() const {
    std::lock_guard<std::mutex> lock(_mtx);
    return _works.size();
}

int LogicSystem::ShardForSessionLocked(const std::string& session_id) const {
    if (_works.empty()) {
        return -1;
    }
    return static_cast<int>(std::hash<std::string>{}(session_id) % _works.size());
}

void LogicSystem::Stop() {
    std::vector<std::unique_ptr<LogicWork>> works;
    {
        std::lock_guard<std::mutex> lock(_mtx);
        if (_b_stop_push.exchange(true, std::memory_order_acq_rel)) {
            return;
        }
        works.swap(_works);
    }
}

LogicSystem::~LogicSystem() {
    Stop();
}

LogicWork::LogicWork() {
    _queue_limit = ResolveLogicQueueLimit();
    initHandle();
    _worker_thread = std::thread(&LogicWork::DealMsg, this);
}

bool LogicWork::PostLogicNode(std::shared_ptr<LogicNode> logic_node) {
    if (!logic_node) {
        return false;
    }
    return PostTask([this, logic_node = std::move(logic_node)]() { HandleMessage(logic_node); });
}

bool LogicWork::PostTask(Task task) {
    std::lock_guard<std::mutex> lock(_mtx);
    if (_b_stop || !task) {
        return false;
    }
    if (_task_queue.size() >= _queue_limit) {
        return false;
    }
    _task_queue.push(std::move(task));
    _cv.notify_one();
    return true;
}

LogicWork::~LogicWork() {
    {
        std::lock_guard<std::mutex> lock(_mtx);
        _b_stop = true;
    }
    _cv.notify_all();
    if (_worker_thread.joinable()) {
        _worker_thread.join();
    }

    std::queue<Task> empty;
    {
        std::lock_guard<std::mutex> lock(_mtx);
        std::swap(_task_queue, empty);
    }
}

void LogicWork::DealMsg() {
    for (;;) {
        Task task;
        {
            std::unique_lock<std::mutex> lock(_mtx);
            _cv.wait(lock, [this] { return _b_stop || !_task_queue.empty(); });
            if (_b_stop) {
                return;
            }
            task = std::move(_task_queue.front());
            _task_queue.pop();
        }

        try {
            task();
        } catch (const std::exception& e) {
            std::cerr << "LogicWork task exception: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "LogicWork task unknown exception" << std::endl;
        }
    }
}

void LogicWork::HandleMessage(const std::shared_ptr<LogicNode>& logic_node) {
    if (!logic_node || !logic_node->_session || !logic_node->_recv_node) {
        return;
    }

    const auto it = _handlers.find(logic_node->_recv_node->_msg_id);
    if (it == _handlers.end()) {
        std::cout << "msg id [" << logic_node->_recv_node->_msg_id << "] handler not found" << std::endl;
        return;
    }

    it->second(
            logic_node->_session, logic_node->_recv_node->_msg_id,
            std::string(logic_node->_recv_node->_data, logic_node->_recv_node->_cur_len)
    );
}

void LogicWork::initHandle() {
    _handlers[MSG_ID::ID_CHAT_LOGIN_QEQ] =
            [](std::shared_ptr<CSession> session, std::uint16_t, const std::string& msg_data) {
                service::LoginService::Handle(session, msg_data);
            };
}
