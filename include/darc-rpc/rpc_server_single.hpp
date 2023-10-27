#ifndef DARC_RPC_RPC_SERVER_SINGLE_HPP__
#define DARC_RPC_RPC_SERVER_SINGLE_HPP__

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>

#include "darc-rpc/rpc_server.hpp"

namespace dc {

template <typename input_message_type, typename output_message_type>
class rpc_server_single {
 public:
  rpc_server_single(const char* address, const uint16_t port,
                    const size_t max_msg_buf_size = cfg::msg_buffer_size)
      : method_(0x0000), keep_running_(false), frag_(max_msg_buf_size) {
    conn_ = new tcp_server(address, port);
  }

  ~rpc_server_single() {
    if (conn_ != nullptr) {
      delete conn_;
      conn_ = nullptr;
    }
  }

  void register_method(std::function<void(input_message_type*, output_message_type*)> callback) {
    callback_ = callback;
  }

  inline void stop() { keep_running_ = false; }

  void run() {
    keep_running_ = true;

    while (keep_running_) {
      if (!conn_->listen()) {
        printf("[rpc_server_single] (method: %" PRIu16 ") trying to open socket again: %s:%" PRIu16
               "\n",
               method_, conn_->connection_info().address, conn_->connection_info().port);
        conn_->close();
        break;
      }

      conn_info client;
      while (conn_->is_active() && keep_running_) {
        ret_accept res = conn_->try_accept(client, cfg::timeout_accept);
        if (res == RET_ACCEPT_FAIL && keep_running_ == false) {
          break;
        } else if (res == RET_ACCEPT_TIMEOUT) {
          continue;
        }

        printf("[rpc_server_single] new client connected: ");
        client.print();
        printf("\n");

        client_loop(client);
      }
    }
  }

 private:
  void client_loop(const conn_info& client) {
    input_message_type input_param;
    output_message_type output_param;

    while (keep_running_ && conn_->is_active()) {
      if (rpc_recv_msg(method_, command::REQ_SEND_INPUT, command::REP_SEND_INPUT, keep_running_,
                       conn_, client.socket_id, tx_, rx_, frag_)) {
        reinterpret_cast<msg_serializer*>(&input_param)
            ->deserialize(frag_.buffer, frag_.buffer_size);

        callback_(&input_param, &output_param);

        frag_.buffer_size = frag_.max_size;
        reinterpret_cast<msg_serializer*>(&output_param)
            ->serialize(frag_.buffer, &frag_.buffer_size);

        rpc_send_msg(method_, command::REQ_SEND_INPUT, command::REP_SEND_INPUT, keep_running_,
                     conn_, client.socket_id, tx_, rx_, frag_);
      }
    }
  }

  // class attributes

  uint16_t method_;
  std::atomic<bool> keep_running_;

  std::function<void(input_message_type*, output_message_type*)> callback_;
  tcp_server* conn_;

  packet_builder tx_;
  packet_parser rx_;
  rpc_frag frag_;
};

}  // namespace dc

#endif  // DARC_RPC_RPC_SERVER_SINGLE_HPP__
