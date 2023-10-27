#ifndef DARC_RPC_RPC_CLIENT_SINGLE_HPP__
#define DARC_RPC_RPC_CLIENT_SINGLE_HPP__

#include "darc-rpc/rpc_server.hpp"

namespace dc {

class rpc_client_single {
 public:
  rpc_client_single(const char* address, const uint16_t port,
                    const size_t max_msg_buf_size = cfg::msg_buffer_size)
      : keep_running_(false), frag_(max_msg_buf_size) {
    conn_ = new tcp_client(address, port);
  }

  ~rpc_client_single() {
    conn_->close();
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

  bool execute(msg_serializer* input, msg_serializer* output) {
    if (!conn_->is_active()) {
      fprintf(stderr, "[rpc_client_single] connection is not active\n");
      return false;
    }

    input->serialize(frag_.buffer, &frag_.buffer_size);

    if (!rpc_send_msg(0x0000, command::REQ_SEND_INPUT, command::REP_SEND_INPUT, keep_running_,
                      conn_, 0, tx_, rx_, frag_)) {
      printf("[rpc_client_single] fail to send msg\n");
      return false;
    }

    if (!rpc_recv_msg(0x0000, command::REQ_SEND_INPUT, command::REP_SEND_INPUT, keep_running_,
                      conn_, 0, tx_, rx_, frag_)) {
      printf("[rpc_client_single] fail to recv msg\n");
      return false;
    }

    if (!output->deserialize(frag_.buffer, frag_.buffer_size)) {
      printf("[rpc_client_single] fail to deserialize\n");
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

#endif  // DARC_RPC_RPC_CLIENT_SINGLE_HPP__
