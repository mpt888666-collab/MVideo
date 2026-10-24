//
// Created by mpt on 2026/10/20.
//

#include "net/MsgNode.h"

#include "const.h"

RecvNode::RecvNode(const uint16_t msg_id, const uint16_t max_len)
    : MsgNode(max_len),
      _msg_id(msg_id) {}

SendNode::SendNode(const char* msg, uint16_t msg_id, uint16_t max_len)
    : MsgNode(max_len + HEAD_TOTAL_LEN),
      _msg_id(msg_id) {
    const uint16_t id = boost::asio::detail::socket_ops::host_to_network_short(msg_id);
    const uint16_t len = boost::asio::detail::socket_ops::host_to_network_short(max_len);

    memcpy(_data, &id, HEAD_ID_LEN);
    memcpy(_data + HEAD_ID_LEN, &len, HEAD_DATA_LEN);
    memcpy(_data + HEAD_TOTAL_LEN, msg, max_len);
}
