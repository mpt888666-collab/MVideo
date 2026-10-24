//
// Logic thread pool and message dispatch.
//

#ifndef CHATSERVER1_LOGICSYSTEM_H
#define CHATSERVER1_LOGICSYSTEM_H

#include "common/Singleton.h"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

class LogicNode;
class CSession;

class LogicWork {
public:
    using Task = std::function<void()>;

    LogicWork();
    ~LogicWork();

    bool PostLogicNode(std::shared_ptr<LogicNode> logic_node);
    bool PostTask(Task task);

private:
    std::mutex _mtx;
    std::condition_variable _cv;
    std::queue<Task> _task_queue;
    std::thread _worker_thread;
    bool _b_stop{false};
    std::size_t _queue_limit{1024};
    std::unordered_map<std::uint16_t, std::function<void(std::shared_ptr<CSession>, std::uint16_t, const std::string&)>>
            _handlers;

    void DealMsg();
    void HandleMessage(const std::shared_ptr<LogicNode>& logic_node);
    void initHandle();
};

class LogicSystem : public Singleton<LogicSystem> {
    friend class Singleton<LogicSystem>;

public:
    ~LogicSystem();

    bool PostLogicQueue(std::shared_ptr<LogicNode> logic_node);
    bool PostContinuation(const std::string& session_id, std::function<void()> task);
    int ShardForSession(const std::string& session_id) const;
    std::size_t WorkerCount() const;

private:
    LogicSystem();

    void Stop();
    int ShardForSessionLocked(const std::string& session_id) const;

    mutable std::mutex _mtx;
    std::atomic<bool> _b_stop_push{false};
    std::vector<std::unique_ptr<LogicWork>> _works;
};

#endif  // CHATSERVER1_LOGICSYSTEM_H
