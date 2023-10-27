#ifndef DARC_RPC_RPC_SERVER_HPP__
#define DARC_RPC_RPC_SERVER_HPP__

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <unordered_map>

#include "darc-rpc/config.hpp"
#include "darc-rpc/frag.hpp"
#include "darc-rpc/protocol.hpp"
#include "darc-rpc/serializer.hpp"
#include "darc-rpc/socket.hpp"

namespace dc {

bool rpc_send_msg(const uint16_t method, uint8_t cmd_req, const uint8_t cmd_rep,
                  std::atomic<bool> &keep_running, socket_transceiver *conn, const SOCKET socket_id,
                  packet_builder &tx, packet_parser &rx, rpc_frag &frag) {
  const size_t max_part_size = cfg::packet_buffer_size - 100;

  size_t file_pos = 0;

  while (keep_running && conn->is_active() && file_pos <= frag.buffer_size) {
    if (cfg::print_msg_frag) printf("[rpc_send_msg] loop \n");

    size_t data_size = max_part_size;

    if (file_pos + data_size > frag.buffer_size) {
      data_size = frag.buffer_size - file_pos;
    }

    if (cfg::print_msg_frag) {
      printf("[rpc_send_msg] file_pos: %zu\n", file_pos);
      printf("[rpc_send_msg] data_size:  %zu\n", data_size);
      printf("[rpc_send_msg] buffer_size:  %zu\n", frag.buffer_size);
    }

    tx.build_data_packet_req(cmd_req, method, static_cast<uint32_t>(file_pos),
                             static_cast<uint32_t>(frag.buffer_size),
                             static_cast<uint16_t>(data_size), frag.buffer + file_pos);

    int tx_tries = 5;
    while (--tx_tries > 0) {
      if (cfg::print_msg_frag) printf("[rpc_send_msg] tx_tries %d\n", tx_tries);

      timer_perf t;
      t.tic();

      if (conn->send(socket_id, tx.buffer(), tx.size()) == RET_SEND_FAIL) {
        fprintf(stderr, "[rpc_send_msg] error: fail to SEND msg part: %zu/%zu\n", file_pos,
                frag.buffer_size);
        return false;
      }

      if (cfg::print_msg_frag) printf("[rpc_send_msg] send time: %0.6f\n", t.toc());

      // rx.rx_size = cfg::recv_buffer_size;
      rx.rx_size = 20;

      t.tic();
      ret_recv res = conn->try_recv(socket_id, rx.rx_buf, &rx.rx_size, cfg::timeout_recv);
      if (cfg::print_msg_frag) printf("[rpc_send_msg] try recv: %0.6f\n", t.toc());

      if (res == RET_RECV_FAIL) {
        fprintf(stderr, "[rpc_send_msg] error: fail to recv msg part (socket): %zu/%zu\n", file_pos,
                frag.buffer_size);
        return false;
      } else if (res == RET_RECV_TIMEOUT) {
        fprintf(stderr, "[rpc_send_msg] error: fail to recv msg part (timeout): %zu/%zu\n",
                file_pos, frag.buffer_size);
        if (!conn->is_socket_active(socket_id)) {
          fprintf(stderr, "[rpc_send_msg] error: socket is innactive");
          return false;
        }
        continue;
      }

      if (rx.parse() && rx.cmd() == cmd_rep) {
        if (cfg::print_rx_packets) {
          rx.print();
        }

        if (rx.result_success()) {
          tx_tries = 5;
          break;
        } else if (rx.result_retry()) {
          printf("[rpc_send_msg] retry: tx_tries %d\n", tx_tries);
          continue;
        } else {
          fprintf(stderr, "[rpc_send_msg] error: fail to send part (abort): %zu/%zu\n", file_pos,
                  frag.buffer_size);
          return false;
        }

        break;
      }
    }

    if (tx_tries <= 0) {
      fprintf(stderr,
              "[rpc_send_msg] error: fail to send msg part (tries exceed): "
              "%zu/%zu\n",
              file_pos, frag.buffer_size);
      return false;
    }

    file_pos += data_size;
    if (file_pos >= frag.buffer_size) {
      break;
    }
  }
  if (cfg::print_msg_frag) printf("[rpc_send_msg] end\n");

  return true;
}

bool rpc_recv_msg(const uint16_t method, uint8_t cmd_req, const uint8_t cmd_rep,
                  std::atomic<bool> &keep_running, socket_transceiver *conn, SOCKET socket_id,
                  packet_builder &tx, packet_parser &rx, rpc_frag &frag) {
  int rx_tries = 5;
  frag.reset();

  while (--rx_tries > 0 && keep_running && conn->is_active()) {
    if (cfg::print_msg_frag) printf("[rpc_recv_msg] rx_tries %d\n", rx_tries);

    rx.rx_size = cfg::packet_buffer_size;

    ret_recv res = conn->try_recv(socket_id, rx.rx_buf, &rx.rx_size, cfg::timeout_recv);
    if (res == RET_RECV_FAIL) {
      fprintf(stderr, "[rpc_recv_msg] error: fail to recv msg req (socket error)");
      return false;
    } else if (res == RET_RECV_TIMEOUT) {
      fprintf(stderr, "[rpc_recv_msg] error: fail to recv msg part (timeout exceed)\n");
      if (!conn->is_socket_active(socket_id)) {
        return false;
      }
      continue;
    }

    if (!rx.parse()) {
      fprintf(stderr, "[rpc_recv_msg] error: fail to parse recv packet\n");
      return false;
    }

    if (cfg::print_rx_packets) {
      rx.print();
    }

    uint8_t result = rep_result::success;

    if (rx.cmd() != cmd_req) {
      fprintf(stderr, "[rpc_recv_msg] error: unexpected command (%u != %u)\n", rx.cmd(), cmd_rep);
      result = rep_result::fail;
    }

    if (rx.method() != method) {
      fprintf(stderr, "[rpc_recv_msg] error: invalid method\n");
      result = rep_result::fail;
    }

    if (rx.file_size() > frag.max_size) {
      fprintf(stderr, "[rpc_recv_msg] error: file size is too big(%" PRIu32 ") %zu\n",
              rx.file_pos(), frag.max_size);
      result = rep_result::fail;
    }

    if (frag.buffer_size == 0 && rx.file_pos() > 0) {
      fprintf(stderr, "[rpc_recv_msg] error: inital file_pos must be zero (%" PRIu32 ")\n",
              rx.file_pos());
      result = rep_result::retry;
    } else if (frag.buffer_size + static_cast<size_t>(rx.buffer_size()) > frag.max_size) {
      fprintf(stderr, "[rpc_recv_msg] error: invalid file_pos and max_size (%" PRIu32 ")\n",
              rx.file_pos());
      result = rep_result::retry;
    } else if (frag.buffer_size + static_cast<size_t>(rx.buffer_size()) > rx.file_size()) {
      fprintf(stderr, "[rpc_recv_msg] error: invalid file_pos and max_size (%" PRIu32 ")\n",
              rx.file_pos());
      result = rep_result::retry;
    }

    if (result == rep_result::success) {
      std::memcpy(frag.buffer + frag.buffer_size, rx.buffer(),
                  static_cast<size_t>(rx.buffer_size()));
      frag.buffer_size += static_cast<size_t>(rx.buffer_size());
    }

    tx.build_data_packet_rep(cmd_rep, method, rx.file_pos(), rx.file_size(), rx.buffer_size(),
                             result);
    conn->send(socket_id, tx.buffer(), tx.size());

    if (result == rep_result::fail) {
      return false;
    }

    if (static_cast<size_t>(rx.file_size()) == frag.buffer_size) {
      if (cfg::print_msg_frag) printf("[rpc_recv_msg] recv finished! %d\n", rx_tries);
      break;
    }
    rx_tries = 5;
  }

  if (cfg::print_msg_frag) printf("[rpc_recv_msg] end\n");
  return true;
}

template <typename input_message_type, typename output_message_type>
class rpc_server {
 public:
  rpc_server(const char *address, const uint16_t port,
             const size_t max_msg_buf_size = cfg::msg_buffer_size)
      : keep_running_(false), frag_(max_msg_buf_size) {
    conn_ = new tcp_server(address, port);
  }

  ~rpc_server() {
    if (conn_ != nullptr) {
      delete conn_;
      conn_ = nullptr;
    }
  }

  void register_method(const uint16_t method,
                       std::function<void(input_message_type *, output_message_type *)> callback) {
    callbacks_[method] = callback;
  }

  void unregister_method(const uint16_t method) {
    auto it = callbacks_.find(method);
    if (it != callbacks_.end()) {
      callbacks_.erase(it);
    }
  }

  inline void stop() { keep_running_ = false; }

  void run() {
    keep_running_ = true;

    while (keep_running_) {
      if (!conn_->listen()) {
        printf("[rcp_server] trying to open socket again: %s:%" PRIu16 "\n",
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

        printf("[rpc_server] new client connected: ");
        client.print();
        printf("\n");

        client_loop(client);
      }
    }
  }

 private:
  void client_loop(const conn_info &client) {
    while (keep_running_ && conn_->is_active()) {
      rx_.rx_size = 9;

      ret_recv res = conn_->try_recv(client, rx_.rx_buf, &rx_.rx_size, cfg::timeout_recv);

      input_message_type input_param;
      output_message_type output_param;

      if (res == RET_RECV_FAIL) {
        fprintf(stderr, "[rpc_server] error: client disconnected\n");
        break;
      } else if (res == RET_RECV_TIMEOUT) {
        if (!conn_->is_client_active(client)) {
          fprintf(stderr, "[rpc_server] error: client disconnected\n");
          break;
        }
        continue;
      }

      if (!rx_.parse()) {
        fprintf(stderr, "[rpc_server] error: fail to parse packet\n");
        continue;
      }

      if (cfg::print_rx_packets) {
        rx_.print();
      }

      const uint16_t method = rx_.method();

      if (rx_.cmd() == command::REQ_EXEC_FUNC && callbacks_.find(method) != callbacks_.end()) {
        tx_.rep_exec_func(rx_.method(), rep_result::success);
        if (conn_->send(client, tx_.buffer(), tx_.size()) == RET_SEND_FAIL) {
          fprintf(stderr, "[rpc_server] error: fail to send REP_EXEC_FUNC\n");
          break;
        }

        if (rpc_recv_msg(method, command::REQ_SEND_INPUT, command::REP_SEND_INPUT, keep_running_,
                         conn_, client.socket_id, tx_, rx_, frag_)) {
          reinterpret_cast<msg_serializer *>(&input_param)
              ->deserialize(frag_.buffer, frag_.buffer_size);

          callbacks_[method](&input_param, &output_param);

          frag_.buffer_size = frag_.max_size;
          reinterpret_cast<msg_serializer *>(&output_param)
              ->serialize(frag_.buffer, &frag_.buffer_size);

          rpc_send_msg(method, command::REQ_SEND_INPUT, command::REP_SEND_INPUT, keep_running_,
                       conn_, client.socket_id, tx_, rx_, frag_);
        }
      } else {
        fprintf(stderr, "[rpc_server] error: invalid request\n");
        tx_.rep_exec_func(rx_.method(), rep_result::fail);
        if (conn_->send(client, tx_.buffer(), tx_.size()) == RET_SEND_FAIL) {
          break;
        }
      }
    }
  }

  // class attributes

  std::unordered_map<uint16_t, std::function<void(input_message_type *, output_message_type *)>>
      callbacks_;

  std::atomic<bool> keep_running_;

  tcp_server *conn_;

  packet_builder tx_;
  packet_parser rx_;
  rpc_frag frag_;
};

}  // namespace dc

#endif  // DARC_RPC_RPC_SERVER_HPP__
