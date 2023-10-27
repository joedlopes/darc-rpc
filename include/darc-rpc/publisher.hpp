#ifndef DARC_RPC_PUBLISHER_HPP__
#define DARC_RPC_PUBLISHER_HPP__

#include "darc-rpc/rpc_server.hpp"

namespace dc {

template <typename message_type>
class publisher {
 public:
  publisher(const char* address, const uint16_t port,
            const size_t max_msg_buf_size = cfg::msg_buffer_size)
      : keep_running_(false), frag_(max_msg_buf_size) {
    conn_ = new tcp_client(address, port);
  }

  ~publisher() {
    if (conn_ != nullptr) {
      delete conn_;
      conn_ = nullptr;
    }
  }

  bool connect() {
    if (conn_->is_active()) {
      return true;
    }

    if (conn_->connect()) {
      keep_running_ = true;
      return true;
    }

    return false;
  }

  bool reset_connection() {
    conn_->close();
    return connect();
  }

  bool is_active() { return conn_->is_active(); }

  bool publish(message_type* input) {
    if (!conn_->is_active()) {
      fprintf(stderr, "[publisher] connection is not active\n");
      return false;
    }

    reinterpret_cast<msg_serializer*>(input)->serialize(frag_.buffer, &frag_.buffer_size);

    if (!rpc_send_msg(0, command::REQ_SEND_INPUT, command::REP_SEND_INPUT, keep_running_, conn_, 0,
                      tx_, rx_, frag_)) {
      printf("[publisher] fail to send msg\n");
      return false;
    }

    return true;
  }

 private:
  std::atomic<bool> keep_running_;

  tcp_client* conn_;

  packet_builder tx_;
  packet_parser rx_;
  rpc_frag frag_;
};

}  // namespace dc

#endif  // DARC_RPC_PUBLISHER_HPP__
