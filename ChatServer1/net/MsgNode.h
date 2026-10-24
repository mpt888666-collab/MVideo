//
// Created by mpt on 2026/10/20.
//

#ifndef CHATSERVER1_MSGNODE_H
#define CHATSERVER1_MSGNODE_H
#include <cstdint>
#include <cstring>
class CSession;
class LogicWork;

class MsgNode {
    friend class CSession;
    friend class SendNode;
    friend class LogicWork;

public:
    MsgNode(const MsgNode&) = delete;
    MsgNode& operator=(const MsgNode&) = delete;

    explicit MsgNode(uint16_t max_len)
        : _cur_len(0),
          _total_len(max_len) {
        _data = new char[max_len + 1]();
        _data[max_len] = '\0';
    }

    ~MsgNode() {
        delete[] _data;
    }

    void Clear() {
        std::memset(_data, '\0', _total_len);
    }

private:
    uint16_t _cur_len;
    uint16_t _total_len;

    char* _data{};
};

class RecvNode : public MsgNode {
    friend class LogicWork;

public:
    RecvNode(uint16_t msg_id, uint16_t max_len);

private:
    uint16_t _msg_id;
};

class SendNode : public MsgNode {
    friend class LogicWork;

public:
    SendNode(const char* msg, uint16_t msg_id, uint16_t max_len);

private:
    uint16_t _msg_id;
};

#endif  // CHATSERVER1_MSGNODE_H
