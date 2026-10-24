//
// A unit of work handed from the network layer to a LogicWork thread.
//

#ifndef CHATSERVER1_LOGICNODE_H
#define CHATSERVER1_LOGICNODE_H

#include <memory>
#include <utility>

class CSession;
class RecvNode;

class LogicNode {
    friend class LogicSystem;
    friend class LogicWork;

public:
    LogicNode(std::shared_ptr<CSession> session, std::shared_ptr<RecvNode> recv_node)
        : _session(std::move(session)),
          _recv_node(std::move(recv_node)) {}

private:
    std::shared_ptr<CSession> _session;
    std::shared_ptr<RecvNode> _recv_node;
};

#endif  // CHATSERVER1_LOGICNODE_H
