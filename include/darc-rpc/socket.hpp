#ifndef DARC_RPC_SOCKET_HPP__
#define DARC_RPC_SOCKET_HPP__

#include <stdio.h>

#include <cinttypes>
#include <cstdint>
#include <cstring>
#include <vector>

#if defined(_MSC_VER)
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif

#ifdef _WIN32

#include <WS2tcpip.h>
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")

#ifndef SHUT_RDWR
#define SHUT_RDWR SD_BOTH
#endif

#else

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#endif

#include "darc-rpc/config.hpp"

namespace dc {

// startup for winsockets
// --------------------------------------------------------------------------

#if _WIN32

class socket_requirements {
 public:
  socket_requirements() {
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
      printf("[WINSOCKET] Failed to initialize Winsock\n");
    } else {
      printf("[WINSOCKET] Winsock Initialized\n");
    }
  }

  ~socket_requirements() { clean_up(); }

  void clean_up() { WSACleanup(); }

  static socket_requirements &init() {
    static socket_requirements instance;
    return instance;
  }

 private:
  WSADATA wsaData;
};

#else

class socket_requirements {
 public:
  socket_requirements() {}
  ~socket_requirements() {}
  void clean_up() {}
  static socket_requirements &init() {
    static socket_requirements instance;
    return instance;
  }
};

typedef int SOCKET;
#define SOCKET_ERROR -1

#endif

// returns
// -----------------------------------------------------------------------------------------

enum ret_recv {
  RET_RECV_SUCCESS = 0,  // Received a packet with sucess
  RET_RECV_FAIL = 1,     // Fail to receive packet -> socket will be terminated
  RET_RECV_TIMEOUT = 2   // Fail to receive packet: timeout
};

enum ret_accept {
  RET_ACCEPT_SUCCESS = 0,  // New client connected
  RET_ACCEPT_FAIL = 1,     // Fail to accept client -> socket will be terminated
  RET_ACCEPT_TIMEOUT = 2   // Fail to accept client: timeout
};

enum ret_send {
  RET_SEND_SUCCESS = 0,  // packet sent with success
  RET_SEND_FAIL = -1     // packet fail to send - disconnect client
};

// conn info
// ---------------------------------------------------------------------------------------

struct conn_info {
  SOCKET socket_id;
  char address[INET_ADDRSTRLEN];
  uint16_t port;

  conn_info() : socket_id(0), port(0) {
    std::memset(address, 0, INET_ADDRSTRLEN);
  }

  conn_info(SOCKET socket_id_, const char *address_, uint16_t port_)
      : socket_id(socket_id_), port(port_) {
    std::memcpy(address, address_, INET_ADDRSTRLEN);
  }

  conn_info(const conn_info &other)
      : socket_id(other.socket_id), port(other.port) {
    std::memcpy(address, other.address, INET_ADDRSTRLEN);
  }

  void print() {
    printf("(socket: %d) %s:%" PRIu16, (int)socket_id, address, port);
  }
};

// conn_interface
// ----------------------------------------------------------------------------------

class socket_transceiver {
 public:
  virtual ret_recv recv(SOCKET socket_id, uint8_t *buffer,
                        size_t *buf_size) = 0;
  virtual ret_recv try_recv(SOCKET socket_id, uint8_t *buffer, size_t *buf_size,
                            const int timeout_millis) = 0;
  virtual ret_send send(SOCKET socket_id, const uint8_t *buffer,
                        size_t buf_size) = 0;

  virtual bool is_socket_active(SOCKET socket_id) = 0;
  virtual bool is_active() = 0;
};

// tcp client
// --------------------------------------------------------------------------------------

class tcp_client : public socket_transceiver {
 public:
  tcp_client(const char *server_address, uint16_t port)
      : conn_(0, server_address, port) {}

  ~tcp_client() { close(); }

  bool connect() {
    if (is_active()) {
      return false;
    }

    conn_.socket_id = socket(AF_INET, SOCK_STREAM, 0);
    if (conn_.socket_id == -1) {
      fprintf(stderr, "[tcp_client] error: Could not create socket\n");
      return false;
    }

    int flag = 1;
    setsockopt(conn_.socket_id, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int));

    int send_buffer_size = cfg::socket_buffer_sizes;
    if (setsockopt(conn_.socket_id, SOL_SOCKET, SO_SNDBUF, &send_buffer_size,
                   sizeof(send_buffer_size)) == -1) {
      ::close(conn_.socket_id);
      return false;
    }

    int receive_buffer_size = cfg::socket_buffer_sizes;
    if (setsockopt(conn_.socket_id, SOL_SOCKET, SO_RCVBUF, &receive_buffer_size,
                   sizeof(receive_buffer_size)) == -1) {
      ::close(conn_.socket_id);
      return 1;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(conn_.port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (inet_pton(AF_INET, conn_.address, &(server_addr.sin_addr)) <= 0) {
      fprintf(stderr, "[tcp_client] error: Invalid IP address: %s\n",
              conn_.address);
      return false;
    }

    if (::connect(conn_.socket_id, (struct sockaddr *)&server_addr,
                  sizeof(server_addr)) != 0) {
      fprintf(stderr, "[tcp_client] error: failed to connect\n");
      return false;
    }

    return true;
  }

  bool close() {
    if (conn_.socket_id >= 0) {
      shutdown(conn_.socket_id, SHUT_RDWR);
#ifdef _WIN32
      closesocket(conn_.socket_id);
#else
      ::close(conn_.socket_id);
#endif
      conn_.socket_id = -1;
      return true;
    }
    return false;
  }

  ret_send send(const uint8_t *buffer, size_t buf_size) {
    if (cfg::print_msg_frag) printf("[tcp_client] send: %zu\n", buf_size);

#if _WIN32
    int res;
    res = ::send(conn_.socket_id, reinterpret_cast<const char *>(buffer),
                 static_cast<int>(buf_size), 0);
#else
    ssize_t res;
    res = ::send(conn_.socket_id, buffer, buf_size, 0);
#endif

    if (res == -1) {
      close();
      return RET_SEND_FAIL;
    } else if (res >= 0 && (static_cast<size_t>(res) != buf_size)) {
      fprintf(stderr, "[tcp_client] error: Fail to send data: size invalid");
      close();
      return RET_SEND_FAIL;
    }

    return RET_SEND_SUCCESS;
  }

  ret_recv recv(uint8_t *buffer, size_t *buf_size) {
    if (cfg::print_msg_frag) printf("[tcp_client] recv: %zu\n", *buf_size);

#if _WIN32
    int res;
    res = ::recv(conn_.socket_id, reinterpret_cast<char *>(buffer),
                 static_cast<int>(*buf_size), 0);
#else
    ssize_t res;
    res = ::recv(conn_.socket_id, buffer, *buf_size, 0);
#endif

    if (res <= 0) {
      fprintf(stderr,
              "[tcp_client] error: fail to receive data from socket (%d)\n",
              (int)conn_.socket_id);
      close();
      return RET_RECV_FAIL;
    }
    *buf_size = static_cast<size_t>(res);
    return RET_RECV_SUCCESS;
  }

  ret_recv try_recv(uint8_t *buffer, size_t *buf_size,
                    const int timeout_millis) {
#ifdef _WIN32
    WSAPOLLFD fds = {};
    fds.fd = conn_.socket_id;
    fds.events = POLLRDNORM;
    fds.revents = 0;
    int ready = WSAPoll(&fds, 1, timeout_millis);
    if (ready > 0 && (fds.revents & POLLRDNORM)) {
#else
    struct pollfd fds;
    fds.fd = conn_.socket_id;
    fds.events = POLLRDNORM;
    fds.revents = 0;
    int ready = poll(&fds, 1, timeout_millis);
    if (false || ready > 0 && (fds.revents & POLLRDNORM)) {
#endif
      return recv(buffer, buf_size);
    }

    return RET_RECV_TIMEOUT;
  }

  inline virtual bool is_active() override { return conn_.socket_id > 0; }

  inline conn_info &connection_info() { return conn_; }

  // socket_transceiver -> client ignores socket_id and default socket

  virtual ret_recv recv(SOCKET socket_id, uint8_t *buffer,
                        size_t *buf_size) override {
    (void)(socket_id);
    return recv(buffer, buf_size);
  }

  virtual ret_recv try_recv(SOCKET socket_id, uint8_t *buffer, size_t *buf_size,
                            const int timeout_millis) override {
    (void)(socket_id);
    return try_recv(buffer, buf_size, timeout_millis);
  }

  virtual ret_send send(SOCKET socket_id, const uint8_t *buffer,
                        size_t buf_size) override {
    (void)(socket_id);
    return send(buffer, buf_size);
  }
  virtual bool is_socket_active(SOCKET socket_id) override {
    (void)(socket_id);
    return is_active();
  }

 private:
  conn_info conn_;
};

// tcp server
// --------------------------------------------------------------------------------------

class tcp_server : public socket_transceiver {
 public:
  tcp_server(const char *address, uint16_t port) : conn_(0, address, port) {}

  ~tcp_server() { close(); }

  bool close() {
    if (is_active()) {
      {  // disconnect clients
        for (auto &client : clients_) {
          shutdown(client.socket_id, SHUT_RDWR);

#ifdef _WIN32
          closesocket(client.socket_id);
#else
          ::close(client.socket_id);
#endif
        }
        clients_.clear();
      }

      shutdown(conn_.socket_id, SHUT_RDWR);

#ifdef _WIN32
      closesocket(conn_.socket_id);
#else
      ::close(conn_.socket_id);
#endif

      conn_.socket_id = -1;
      return true;
    }
    return false;
  }

  bool listen() {
    if (is_active()) {
      fprintf(stderr, "[tpc_server] error: socket is already active\n");
      return false;
    }

    conn_.socket_id = socket(AF_INET, SOCK_STREAM, 0);
    if (conn_.socket_id == 0) {
      fprintf(stderr, "[tpc_server] error: fail to create socket\n");
      return false;
    }

    int flag = 1;
    setsockopt(conn_.socket_id, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int));

    int send_buffer_size = cfg::socket_buffer_sizes;
    if (setsockopt(conn_.socket_id, SOL_SOCKET, SO_SNDBUF, &send_buffer_size,
                   sizeof(send_buffer_size)) == -1) {
      ::close(conn_.socket_id);
      return false;
    }

    int receive_buffer_size = cfg::socket_buffer_sizes;
    if (setsockopt(conn_.socket_id, SOL_SOCKET, SO_RCVBUF, &receive_buffer_size,
                   sizeof(receive_buffer_size)) == -1) {
      ::close(conn_.socket_id);
      return 1;
    }

#ifdef _WIN32
    const char reuse = 1;
    if (setsockopt(conn_.socket_id, SOL_SOCKET, SO_REUSEADDR, &reuse,
                   sizeof(reuse)) < 0) {
#else
    const int reuse = 1;
    if (setsockopt(conn_.socket_id, SOL_SOCKET, SO_REUSEADDR, &reuse,
                   sizeof(reuse)) < 0) {
#endif
      fprintf(stderr, "[tpc_server] error: fail to set SO_REUSEADDR to 1\n");
      close();
      return false;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(conn_.port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (inet_pton(AF_INET, conn_.address, &(server_addr.sin_addr)) <= 0) {
      fprintf(stderr, "[tpc_server] error: invalid ip address %s\n",
              conn_.address);
      close();
      return false;
    }

    if (::bind(conn_.socket_id, (struct sockaddr *)&server_addr,
               sizeof(server_addr)) == -1) {
      fprintf(stderr, "[tpc_server] Bind failed\n");
      return false;
    }

    if (::listen(conn_.socket_id, 5) == -1) {
      fprintf(stderr, "[tpc_server] Listen failed\n");
      return false;
    }

    return true;
  }

  inline virtual bool is_active() override { return conn_.socket_id > 0; }

  ret_accept accept(conn_info &client) {
    if (!is_active()) {
      return RET_ACCEPT_FAIL;
    }

    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    SOCKET client_socket =
        ::accept(conn_.socket_id, (struct sockaddr *)&client_addr, &addr_len);

    if (client_socket == -1) {
      fprintf(stderr, "[tpc_server] error: fail to accept client\n");

#ifdef _WIN32
      closesocket(conn_.socket_id);
#else
      ::close(conn_.socket_id);
#endif

      return RET_ACCEPT_FAIL;
    }

    char buf[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(client_addr.sin_addr), buf, sizeof(buf));
    client = conn_info(client_socket, buf, client_addr.sin_port);
    on_client_connected(client_socket, buf, client_addr.sin_port);
    return RET_ACCEPT_SUCCESS;
  }

  ret_accept try_accept(conn_info &client, const int timeout_millis) {
    if (!is_active()) {
      return RET_ACCEPT_FAIL;
    }

#ifdef _WIN32
    WSAPOLLFD fds = {};
    fds.fd = conn_.socket_id;
    fds.events = POLLIN;
    fds.revents = 0;
    int ready = WSAPoll(&fds, 1, timeout_millis);
    if (ready > 0 && (fds.revents & POLLIN)) {
#else
    struct pollfd fds;
    fds.fd = conn_.socket_id;
    fds.events = POLLIN;
    fds.revents = 0;
    int ready = poll(&fds, 1, timeout_millis);
    if (false || ready > 0 && (fds.revents & POLLIN)) {
#endif

      return accept(client);
    }

    return RET_ACCEPT_TIMEOUT;
  }

  std::vector<conn_info> clients() { return clients_; }

  virtual ret_send send(SOCKET socket_id, const uint8_t *buffer,
                        size_t buf_size) override {
    if (cfg::print_msg_frag) printf("[tpc_server] send: %zu\n", buf_size);

#if _WIN32
    int res;
    res = ::send(socket_id, reinterpret_cast<const char *>(buffer),
                 static_cast<int>(buf_size), 0);
#else
    ssize_t res;
    res = ::send(socket_id, buffer, buf_size, 0);
#endif

    if (res == SOCKET_ERROR) {
      fprintf(stderr, "[tpc_server] error: fail to send data\n");
      disconnect_client(socket_id);
      return RET_SEND_FAIL;
    } else if (res >= 0 && (static_cast<size_t>(res) != buf_size)) {
      fprintf(stderr, "[tpc_server] error: fail to send data (buffer size)\n");
      disconnect_client(socket_id);
      return RET_SEND_FAIL;
    }

    return RET_SEND_SUCCESS;
  }

  inline ret_send send(const conn_info &client, const uint8_t *buffer,
                       size_t buf_size) {
    return this->send(client.socket_id, buffer, buf_size);
  }

  virtual ret_recv recv(SOCKET socket_id, uint8_t *buffer,
                        size_t *buf_size) override {
    if (cfg::print_msg_frag) printf("[tpc_server] recv: %zu\n", *buf_size);

#if _WIN32
    int res = ::recv(socket_id, reinterpret_cast<char *>(buffer),
                     static_cast<int>(*buf_size), 0);
#else
    ssize_t res = ::recv(socket_id, buffer, *buf_size, 0);
#endif

    if (res == SOCKET_ERROR || res <= 0) {
      fprintf(stderr, "[tpc_server] error: fail to receive from socket (%d)\n",
              (int)socket_id);

      disconnect_client(socket_id);
      return RET_RECV_FAIL;
    }
    *buf_size = static_cast<size_t>(res);
    return RET_RECV_SUCCESS;
  }

  inline ret_recv recv(const conn_info &client, uint8_t *buffer,
                       size_t *buf_size) {
    return recv(client.socket_id, buffer, buf_size);
  }

  virtual ret_recv try_recv(SOCKET socket_id, uint8_t *buffer, size_t *buf_size,
                            const int timeout_millis) override {
#ifdef _WIN32
    WSAPOLLFD fds{};
    fds.fd = socket_id;
    fds.events = POLLRDNORM;
    int ready = WSAPoll(&fds, 1, timeout_millis);
#else
    struct pollfd fds;
    fds.fd = socket_id;
    fds.events = POLLRDNORM;
    fds.revents = 0;
    int ready = poll(&fds, 1, timeout_millis);
#endif
    if (false || ready > 0 && (fds.revents & POLLRDNORM)) {
      return recv(socket_id, buffer, buf_size);
    }
    return RET_RECV_TIMEOUT;
  }

  inline ret_recv try_recv(const conn_info &client, uint8_t *buffer,
                           size_t *buf_size, const int timeout_millis) {
    return try_recv(client.socket_id, buffer, buf_size, timeout_millis);
  }

  bool disconnect_client(SOCKET socket_id) {
    if (!is_active()) {
      return false;
    }

    for (size_t i = 0; i < clients_.size(); i++) {
      if (clients_[i].socket_id == socket_id) {
        ::shutdown(clients_[i].socket_id, SHUT_RDWR);

#ifdef _WIN32
        closesocket(clients_[i].socket_id);
#else
        ::close(clients_[i].socket_id);
#endif

        clients_.erase(clients_.begin() + i);
        printf("[tcp_server] client disconnected: %d\n",
               static_cast<int>(socket_id));
        return true;
      }
    }

    return false;
  }

  bool is_client_active(const conn_info &client) {
    for (size_t i = 0; i < clients_.size(); i++) {
      if (clients_[i].socket_id == client.socket_id) {
        return true;
      }
    }
    return false;
  }

  inline bool disconnect_client(const conn_info &client) {
    return disconnect_client(client.socket_id);
  }

  inline size_t total_clients() { return clients_.size(); }

  inline std::vector<conn_info> connected_clients() { return clients_; }

  inline conn_info &connection_info() { return conn_; }

  virtual bool is_socket_active(SOCKET socket_id) override {
    for (size_t i = 0; i < clients_.size(); i++) {
      if (clients_[i].socket_id == socket_id) {
        return true;
      }
    }
    return false;
  }

 private:
  void on_client_connected(SOCKET socket_id, const char *address,
                           uint16_t port) {
    clients_.push_back(conn_info(socket_id, address, port));
  }

  conn_info conn_;
  std::vector<conn_info> clients_;
};

}  // namespace dc

#endif  // DARC_RPC_SOCKET_HPP__
