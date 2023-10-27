#ifndef DARC_RPC_RPC_CLIENT_HPP__
#define DARC_RPC_RPC_CLIENT_HPP__

#include "darc-rpc/rpc_server.hpp"

namespace dc {

class rpc_client {
 public:
  rpc_client(const char* address, const uint16_t port,
             const size_t max_msg_buf_size = cfg::msg_buffer_size)
      : keep_running_(false), frag_(max_msg_buf_size) {
    conn_ = new tcp_client(address, port);
  }

  ~rpc_client() {
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

  bool execute(const uint16_t method, msg_serializer* input, msg_serializer* output) {
    if (!conn_->is_active()) {
      fprintf(stderr, "[rpc_client] connection is not active\n");
      return false;
    }

    if (!send_exec_func(method)) {
      return false;
    }

    input->serialize(frag_.buffer, &frag_.buffer_size);

    if (!rpc_send_msg(method, command::REQ_SEND_INPUT, command::REP_SEND_INPUT, keep_running_,
                      conn_, 0, tx_, rx_, frag_)) {
      printf("[rpc_client] fail to send msg\n");
      return false;
    }

    if (!rpc_recv_msg(method, command::REQ_SEND_INPUT, command::REP_SEND_INPUT, keep_running_,
                      conn_, 0, tx_, rx_, frag_)) {
      printf("[rpc_client] fail to recv msg\n");
      return false;
    }

    if (!output->deserialize(frag_.buffer, frag_.buffer_size)) {
      printf("[rpc_client] fail to deserialize\n");
      return false;
    }

    return true;
  }

 private:
  bool send_exec_func(const uint16_t method) {
    tx_.req_exec_func(method);

    if (conn_->send(tx_.buffer(), tx_.size()) == RET_SEND_FAIL) {
      fprintf(stderr, "[rpc_client] send_exec_func: fail to send\n");
      return false;
    }

    rx_.rx_size = cfg::packet_buffer_size;
    ret_recv res = conn_->try_recv(rx_.rx_buf, &rx_.rx_size, 1000);
    if (res != RET_RECV_SUCCESS) {
      fprintf(stderr, "[rpc_client] send_exec_func: fail to receive\n");
      return false;
    }

    if (!rx_.parse()) {
      fprintf(stderr, "[rpc_client] send_exec_func: invalid packet\n");
      return false;
    }

    if (cfg::print_rx_packets) {
      rx_.print();
    }

    if (rx_.cmd() != command::REP_EXEC_FUNC) {
      fprintf(stderr, "[rpc_client] send_exec_func: unexpected command replied\n");
      return false;
    }

    if (rx_.method() != method) {
      fprintf(stderr,
              "[rpc_client] send_exec_func: invalid method (%" PRIu16 " != %" PRIu16 ")replied\n",
              rx_.method(), method);
      return true;
    }

    if (!rx_.result_success()) {
      fprintf(stderr, "[rpc_client] send_exec_func: method not available\n");
      return false;
    }

    return true;
  }

  std::atomic<bool> keep_running_;

  tcp_client* conn_;

  packet_builder tx_;
  packet_parser rx_;
  rpc_frag frag_;
};

}  // namespace dc

#endif  // DARC_RPC_RPC_CLIENT_HPP__
