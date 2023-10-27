#ifndef DARC_RPC_PUBLISHER_HPP__
#define DARC_RPC_PUBLISHER_HPP__

#include "darc-rpc/rpc_server.hpp"

namespace dc {

template <typename message_type>
class subscriber {
 public:
  subscriber(const char* address, const uint16_t port,
             const size_t max_msg_buf_size = cfg::msg_buffer_size)
      : method_(0), keep_running_(false), frag_(max_msg_buf_size) {
    conn_ = new tcp_server(address, port);
  }

  ~subscriber() {
    if (conn_ != nullptr) {
      delete conn_;
      conn_ = nullptr;
    }
  }

  void register_callback(std::function<void(message_type*)> callback) { callback_ = callback; }

  inline void stop() { keep_running_ = false; }

  void run() {
    keep_running_ = true;

    while (keep_running_) {
      if (!conn_->listen()) {
        printf("[subscriber] trying to open socket again: %s:%" PRIu16 "\n",
               conn_->connection_info().address, conn_->connection_info().port);
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

        printf("[subscriber] new client connected: ");
        client.print();
        printf("\n");

        client_loop(client);
      }
    }
  }

 private:
  void client_loop(const conn_info& client) {
    message_type input_param;
    while (keep_running_ && conn_->is_active()) {
      if (rpc_recv_msg(method_, command::REQ_SEND_INPUT, command::REP_SEND_INPUT, keep_running_,
                       conn_, client.socket_id, tx_, rx_, frag_)) {
        reinterpret_cast<msg_serializer*>(&input_param)
            ->deserialize(frag_.buffer, frag_.buffer_size);

        if (callback_) {
          callback_(&input_param);
        }
      }
    }
  }

  const uint16_t method_;
  std::atomic<bool> keep_running_;

  std::function<void(message_type*)> callback_;
  tcp_server* conn_;

  packet_builder tx_;
  packet_parser rx_;
  rpc_frag frag_;
};

}  // namespace dc

#endif  // DARC_RPC_PUBLISHER_HPP__
