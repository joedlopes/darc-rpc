/*
  MIT License

  Copyright (c) 2023 Joed Lopes da Silva

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS," WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE, AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES, OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT, OR OTHERWISE, ARISING
  FROM, OUT OF, OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
  DEALINGS IN THE SOFTWARE.
*/

/*
  File: darc-rpc.hpp
  Author: Joed Lopes da Silva
  Description: Single-header file for D'Arc RPC
  Repository: http://www.github.com/joedlopes/darc-rpc
  Version: 0.01
*/

#ifndef DARC_RCP_SINGLE_HEADER_HPP__
#define DARC_RCP_SINGLE_HEADER_HPP__

// Headers
// -------------------------------------------------------------------------------------------------
#include <stdio.h>

#include <atomic>
#include <chrono>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <memory>
#include <unordered_map>
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

typedef int SOCKET;
#define SOCKET_ERROR -1

#endif

// D'Arc Namespace
// -----------------------------------------------------------------------------
namespace dc {

    // Config Section
    // -----------------------------------------------------------------------------

#ifndef DARC_RPC_CONFIG
#define DARC_RPC_CONFIG

    namespace cfg {

        // Debug rx packets
        const bool print_msg_frag = false;

        const bool print_rx_packets = false;

        const int timeout_recv = 1000;

        const int timeout_accept = 1000;

        const size_t recv_buffer_size = 1024;

        constexpr size_t packet_buffer_size = recv_buffer_size - 22;

        constexpr size_t msg_buffer_size = 2536 * 2080 * 4;

        constexpr int socket_buffer_sizes = msg_buffer_size;

    };  // namespace cfg

#endif  // DARC_RPC_CONFIG

    // Utils functions for buffer writing and read and time profiling
    // -----------------------------------------------------------------------------

    uint32_t bytes_to_u32(const uint8_t* ptr) {
        return static_cast<uint32_t>(ptr[0]) | (static_cast<uint32_t>(ptr[1]) << 8) |
            (static_cast<uint32_t>(ptr[2]) << 16) |
            (static_cast<uint32_t>(ptr[3]) << 24);
    }

    uint16_t bytes_to_u16(const uint8_t* ptr) {
        return static_cast<uint32_t>(ptr[0]) | (static_cast<uint32_t>(ptr[1]) << 8);
    }

    uint8_t* buffer_write_u32(uint8_t* buffer, const uint32_t value) {
        buffer[0] = static_cast<uint8_t>(value);
        buffer[1] = static_cast<uint8_t>(value >> 8);
        buffer[2] = static_cast<uint8_t>(value >> 16);
        buffer[3] = static_cast<uint8_t>(value >> 24);
        return buffer + sizeof(uint32_t);
    }

    uint8_t* buffer_write_u16(uint8_t* buffer, const uint16_t value) {
        buffer[0] = static_cast<uint8_t>(value);
        buffer[1] = static_cast<uint8_t>(value >> 8);
        return buffer + sizeof(uint16_t);
    }

    uint8_t* buffer_write_u8(uint8_t* buffer, const uint8_t value) {
        buffer[0] = value;
        return buffer + sizeof(uint8_t);
    }

    uint8_t* buffer_write_u8vec(uint8_t* buffer, const uint8_t* data,
        const size_t size) {
        std::memcpy(buffer, data, size);
        return buffer + size;
    }

    class timer_perf {
    public:
        timer_perf() { tic(); }

        void tic() { start_time = std::chrono::high_resolution_clock::now(); }

        float toc() {
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
                end_time - start_time);
            return duration.count() / 1e9f;  // Convert to seconds
        }

    private:
        std::chrono::high_resolution_clock::time_point start_time;
    };

    // Sockets
    // -----------------------------------------------------------------------------

#if _WIN32  // startup for winsockets
    class socket_requirements {
    public:
        socket_requirements() {
            if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
                printf("[WINSOCKET] Failed to initialize Winsock\n");
            }
            else {
                printf("[WINSOCKET] Winsock Initialized\n");
            }
        }
        ~socket_requirements() { clean_up(); }
        void clean_up() { WSACleanup(); }
        static socket_requirements& init() {
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
        static socket_requirements& init() {
            static socket_requirements instance;
            return instance;
        }
    };
#endif

    // enums for return

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

    // socket connection info

    struct conn_info {
        SOCKET socket_id;
        char address[INET_ADDRSTRLEN];
        uint16_t port;

        conn_info() : socket_id(0), port(0) {
            std::memset(address, 0, INET_ADDRSTRLEN);
        }

        conn_info(SOCKET socket_id_, const char* address_, uint16_t port_)
            : socket_id(socket_id_), port(port_) {
            std::memcpy(address, address_, INET_ADDRSTRLEN);
        }

        conn_info(const conn_info& other)
            : socket_id(other.socket_id), port(other.port) {
            std::memcpy(address, other.address, INET_ADDRSTRLEN);
        }

        void print() {
            printf("(socket: %d) %s:%" PRIu16, (int)socket_id, address, port);
        }
    };

    // interface for sending and receiving data

    class socket_transceiver {
    public:
        virtual ret_recv recv(SOCKET socket_id, uint8_t* buffer,
            size_t* buf_size) = 0;
        virtual ret_recv try_recv(SOCKET socket_id, uint8_t* buffer, size_t* buf_size,
            const int timeout_millis) = 0;
        virtual ret_send send(SOCKET socket_id, const uint8_t* buffer,
            size_t buf_size) = 0;

        virtual bool is_socket_active(SOCKET socket_id) = 0;
        virtual bool is_active() = 0;
    };

    // tcp client
    // -----------------------------------------------------------------------------

    class tcp_client : public socket_transceiver {
    public:
        tcp_client(const char* server_address, uint16_t port)
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


#if _WIN32
            char flag = 1;
            if (setsockopt(conn_.socket_id, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(char)) == -1) {
                fprintf(stderr, "[tcp_client] error: fail to set option TCP_NODELAY\n");
                close();
                return false;
            }


            const int txrx_buffer = cfg::socket_buffer_sizes;
            if (setsockopt(conn_.socket_id, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const char*>(&txrx_buffer),
                sizeof(txrx_buffer)) == -1) {
                fprintf(stderr, "[tcp_client] error: fail to set socket tx buffer size\n");
                close();
                return false;
            }

            if (setsockopt(conn_.socket_id, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&txrx_buffer),
                sizeof(txrx_buffer)) == -1) {
                fprintf(stderr, "[tcp_client] error: fail to set socket rx buffer size\n");
                close();
                return 1;
            }

#else
            int flag = 1;
            if (setsockopt(conn_.socket_id, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int)) == -1) {
                fprintf(stderr, "[tcp_client] error: fail to set option TCP_NODELAY\n");
                close();
                return false;
            }

            const int txrx_buffer = cfg::socket_buffer_sizes;
            if (setsockopt(conn_.socket_id, SOL_SOCKET, SO_SNDBUF, &txrx_buffer,
                sizeof(txrx_buffer)) == -1) {
                fprintf(stderr, "[tcp_client] error: fail to set socket tx buffer size\n");
                close();
                return false;
            }
            int receive_buffer_size = cfg::socket_buffer_sizes;
            if (setsockopt(conn_.socket_id, SOL_SOCKET, SO_RCVBUF, &txrx_buffer,
                sizeof(txrx_buffer)) == -1) {
                fprintf(stderr, "[tcp_client] error: fail to set socket rx buffer size\n");
                close();
                return 1;
            }
#endif


            struct sockaddr_in server_addr;
            server_addr.sin_family = AF_INET;
            server_addr.sin_port = htons(conn_.port);
            server_addr.sin_addr.s_addr = INADDR_ANY;

            if (inet_pton(AF_INET, conn_.address, &(server_addr.sin_addr)) <= 0) {
                fprintf(stderr, "[tcp_client] error: Invalid IP address: %s\n",
                    conn_.address);
                return false;
            }

            if (::connect(conn_.socket_id, (struct sockaddr*)&server_addr,
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

        ret_send send(const uint8_t* buffer, size_t buf_size) {
            if (cfg::print_msg_frag) printf("[tcp_client] send: %zu\n", buf_size);

#if _WIN32
            int res;
            res = ::send(conn_.socket_id, reinterpret_cast<const char*>(buffer),
                static_cast<int>(buf_size), 0);
#else
            ssize_t res;
            res = ::send(conn_.socket_id, buffer, buf_size, 0);
#endif

            if (res == -1) {
                close();
                return RET_SEND_FAIL;
            }
            else if (res >= 0 && (static_cast<size_t>(res) != buf_size)) {
                fprintf(stderr, "[tcp_client] error: Fail to send data: size invalid");
                close();
                return RET_SEND_FAIL;
            }

            return RET_SEND_SUCCESS;
        }

        ret_recv recv(uint8_t* buffer, size_t* buf_size) {
            if (cfg::print_msg_frag) printf("[tcp_client] recv: %zu\n", *buf_size);

#if _WIN32
            int res;
            res = ::recv(conn_.socket_id, reinterpret_cast<char*>(buffer),
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

        ret_recv try_recv(uint8_t* buffer, size_t* buf_size,
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

        inline conn_info& connection_info() { return conn_; }

        // socket_transceiver -> client ignores socket_id and default socket

        virtual ret_recv recv(SOCKET socket_id, uint8_t * buffer,
            size_t * buf_size) override {
            (void)(socket_id);
            return recv(buffer, buf_size);
        }

        virtual ret_recv try_recv(SOCKET socket_id, uint8_t * buffer, size_t * buf_size,
            const int timeout_millis) override {
            (void)(socket_id);
            return try_recv(buffer, buf_size, timeout_millis);
        }

        virtual ret_send send(SOCKET socket_id, const uint8_t * buffer,
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
    // -----------------------------------------------------------------------------

    class tcp_server : public socket_transceiver {
    public:
        tcp_server(const char* address, uint16_t port) : conn_(0, address, port) {}

        ~tcp_server() { close(); }

        bool close() {
            if (is_active()) {
                {  // disconnect clients
                    for (auto& client : clients_) {
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

#if _WIN32
            char flag = 1;
            if (setsockopt(conn_.socket_id, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(char)) == -1) {
                fprintf(stderr, "[tcp_client] error: fail to set option TCP_NODELAY\n");
                close();
                return false;
            }


            const int txrx_buffer = cfg::socket_buffer_sizes;
            if (setsockopt(conn_.socket_id, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const char*>(&txrx_buffer),
                sizeof(txrx_buffer)) == -1) {
                fprintf(stderr, "[tcp_client] error: fail to set socket tx buffer size\n");
                close();
                return false;
            }

            if (setsockopt(conn_.socket_id, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&txrx_buffer),
                sizeof(txrx_buffer)) == -1) {
                fprintf(stderr, "[tcp_client] error: fail to set socket rx buffer size\n");
                close();
                return 1;
            }

            const int reuse = 1;
            if (setsockopt(conn_.socket_id, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse),
                sizeof(int)) < 0) {
                fprintf(stderr, "[tpc_server] error: fail to set SO_REUSEADDR to 1\n");
                close();
                return false;
            }

#else
            int flag = 1;
            if (setsockopt(conn_.socket_id, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int)) == -1) {
                fprintf(stderr, "[tcp_client] error: fail to set option TCP_NODELAY\n");
                close();
                return false;
            }

            const int txrx_buffer = cfg::socket_buffer_sizes;
            if (setsockopt(conn_.socket_id, SOL_SOCKET, SO_SNDBUF, &txrx_buffer,
                sizeof(txrx_buffer)) == -1) {
                fprintf(stderr, "[tcp_client] error: fail to set socket tx buffer size\n");
                close();
                return false;
            }
            int receive_buffer_size = cfg::socket_buffer_sizes;
            if (setsockopt(conn_.socket_id, SOL_SOCKET, SO_RCVBUF, &txrx_buffer,
                sizeof(txrx_buffer)) == -1) {
                fprintf(stderr, "[tcp_client] error: fail to set socket rx buffer size\n");
                close();
                return 1;
            }

            const int reuse = 1;
            if (setsockopt(conn_.socket_id, SOL_SOCKET, SO_REUSEADDR, &reuse,
                sizeof(reuse)) < 0) {
                fprintf(stderr, "[tpc_server] error: fail to set SO_REUSEADDR to 1\n");
                close();
                return false;
            }
#endif

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

            if (::bind(conn_.socket_id, (struct sockaddr*)&server_addr,
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

        ret_accept accept(conn_info& client) {
            if (!is_active()) {
                return RET_ACCEPT_FAIL;
            }

            struct sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);
            SOCKET client_socket =
                ::accept(conn_.socket_id, (struct sockaddr*)&client_addr, &addr_len);

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

        ret_accept try_accept(conn_info& client, const int timeout_millis) {
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
            else if (ready > 0 && (fds.revents & POLLERR)) {
                return RET_ACCEPT_FAIL;
            }

            return RET_ACCEPT_TIMEOUT;
            }

        std::vector<conn_info> clients() { return clients_; }

        virtual ret_send send(SOCKET socket_id, const uint8_t * buffer,
            size_t buf_size) override {
            if (cfg::print_msg_frag) printf("[tpc_server] send: %zu\n", buf_size);

#if _WIN32
            int res;
            res = ::send(socket_id, reinterpret_cast<const char*>(buffer),
                static_cast<int>(buf_size), 0);
#else
            ssize_t res;
            res = ::send(socket_id, buffer, buf_size, 0);
#endif

            if (res == SOCKET_ERROR) {
                fprintf(stderr, "[tpc_server] error: fail to send data\n");
                disconnect_client(socket_id);
                return RET_SEND_FAIL;
            }
            else if (res >= 0 && (static_cast<size_t>(res) != buf_size)) {
                fprintf(stderr, "[tpc_server] error: fail to send data (buffer size)\n");
                disconnect_client(socket_id);
                return RET_SEND_FAIL;
            }

            return RET_SEND_SUCCESS;
        }

        inline ret_send send(const conn_info & client, const uint8_t * buffer,
            size_t buf_size) {
            return this->send(client.socket_id, buffer, buf_size);
        }

        virtual ret_recv recv(SOCKET socket_id, uint8_t * buffer,
            size_t * buf_size) override {
            if (cfg::print_msg_frag) printf("[tpc_server] recv: %zu\n", *buf_size);

#if _WIN32
            int res = ::recv(socket_id, reinterpret_cast<char*>(buffer),
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

        inline ret_recv recv(const conn_info & client, uint8_t * buffer,
            size_t * buf_size) {
            return recv(client.socket_id, buffer, buf_size);
        }

        virtual ret_recv try_recv(SOCKET socket_id, uint8_t * buffer, size_t * buf_size,
            const int timeout_millis) override {
#ifdef _WIN32
            WSAPOLLFD fds{};
            fds.fd = socket_id;
            fds.events = POLLIN;
            int ready = WSAPoll(&fds, 1, timeout_millis);
            if (false || ready > 0 && (fds.revents & POLLIN)) {
                return recv(socket_id, buffer, buf_size);
            }
#else
            struct pollfd fds;
            fds.fd = socket_id;
            fds.events = POLLIN;
            fds.revents = 0;
            int ready = poll(&fds, 1, timeout_millis);
            if (false || ready > 0 && (fds.revents & POLLIN)) {
                return recv(socket_id, buffer, buf_size);
            }
#endif
            if (ready > 0 && (fds.revents & POLLERR)) {
                return RET_RECV_FAIL;
            }
            return RET_RECV_TIMEOUT;
        }

        inline ret_recv try_recv(const conn_info & client, uint8_t * buffer,
            size_t * buf_size, const int timeout_millis) {
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

        bool is_client_active(const conn_info & client) {
            for (size_t i = 0; i < clients_.size(); i++) {
                if (clients_[i].socket_id == client.socket_id) {
                    return true;
                }
            }
            return false;
        }

        inline bool disconnect_client(const conn_info & client) {
            return disconnect_client(client.socket_id);
        }

        inline size_t total_clients() { return clients_.size(); }

        inline std::vector<conn_info> connected_clients() { return clients_; }

        inline conn_info& connection_info() { return conn_; }

        virtual bool is_socket_active(SOCKET socket_id) override {
            for (size_t i = 0; i < clients_.size(); i++) {
                if (clients_[i].socket_id == socket_id) {
                    return true;
                }
            }
            return false;
        }

    private:
        void on_client_connected(SOCKET socket_id, const char* address,
            uint16_t port) {
            clients_.push_back(conn_info(socket_id, address, port));
        }

        conn_info conn_;
        std::vector<conn_info> clients_;
        };

    // Framentation of packets
    // -----------------------------------------------------------------------------

    struct rpc_frag {
        const size_t max_size;

        uint8_t* buffer;
        size_t buffer_size;

        rpc_frag(size_t max_buffer_size) : buffer_size(0), max_size(max_buffer_size) {
            buffer = new uint8_t[max_size];  // allocate buffer
            std::memset(buffer, 0, max_size);
        }

        ~rpc_frag() {
            if (buffer != nullptr) {
                delete[] buffer;
                buffer = nullptr;
            }
        }

        void reset() { buffer_size = 0; }
    };

    // Protocol
    // -----------------------------------------------------------------------------

    class command {
    public:
        static const uint8_t REQ_EXEC_FUNC = 1;  // prepare to execute function
        static const uint8_t REP_EXEC_FUNC = 2;
        static const uint8_t REQ_SEND_INPUT = 3;  // send input parts
        static const uint8_t REP_SEND_INPUT = 4;
        static const uint8_t REQ_SEND_OUTPUT = 5;  // receive input parts
        static const uint8_t REP_SEND_OUTPUT = 6;
    };

    class rep_result {
    public:
        static const uint8_t success = 0;  // successfully executed
        static const uint8_t retry = 1;    // try to execute the operation again
        static const uint8_t fail = 2;     // failed, stop entire flow
    };

    /**
     * protocol: it provides a set of functions to create packets
     */
    class packet_builder {
    public:
        packet_builder() : tx_size(0) {
            std::memset(tx_buf, 0, cfg::packet_buffer_size);
        }

        inline const size_t size() { return tx_size; }
        inline const uint8_t* buffer() { return tx_buf; }

        void print_buffer() {
            printf("TX: ");
            for (size_t i = 0; i < tx_size && i < cfg::packet_buffer_size; i++) {
                printf("%02x ", tx_buf[i]);
            }
            printf("\nsize: %zu\n", tx_size);
        }

        // [size:2][cmd:1][method:2][crc:4]
        //    9
        void req_exec_func(const uint16_t method) {
            uint8_t* ptr =
                buffer_write_u8(tx_buf + sizeof(uint16_t), command::REQ_EXEC_FUNC);
            ptr = buffer_write_u16(ptr, method);
            build_end(ptr);
        }

        // [size:2][cmd:1][method:2][result:1][crc:4]
        //    10
        void rep_exec_func(const uint16_t method, const uint8_t result) {
            uint8_t* ptr =
                buffer_write_u8(tx_buf + sizeof(uint16_t), command::REP_EXEC_FUNC);
            ptr = buffer_write_u16(ptr, method);
            ptr = buffer_write_u8(ptr, result);
            build_end(ptr);
        }

        void req_send_input(const uint16_t method, const uint32_t file_pos,
            const uint32_t file_size, const uint16_t buffer_size,
            const uint8_t* buffer) {
            build_data_packet_req(command::REQ_SEND_INPUT, method, file_pos, file_size,
                buffer_size, buffer);
        }

        void rep_send_input(const uint16_t method, const uint32_t file_pos,
            const uint32_t file_size, const size_t buffer_size,
            const uint8_t result) {
            build_data_packet_rep(command::REP_SEND_INPUT, method, file_pos, file_size,
                static_cast<uint16_t>(buffer_size), result);
        }

        void req_send_output(const uint16_t method, const uint32_t file_pos,
            const uint32_t file_size, const uint16_t buffer_size,
            const uint8_t* buffer) {
            build_data_packet_req(command::REQ_SEND_OUTPUT, method, file_pos, file_size,
                static_cast<uint16_t>(buffer_size), buffer);
        }

        void rep_send_output(const uint16_t method, const uint32_t file_pos,
            const uint32_t file_size, const size_t buffer_size,
            const uint8_t result) {
            build_data_packet_rep(command::REP_SEND_OUTPUT, method, file_pos, file_size,
                static_cast<uint16_t>(buffer_size), result);
        }

        // [size:2][cmd:1][method:2][file_pos:4][file_size:4][buffer_size:2][data:1][crc:4]
        // size
        void build_data_packet_req(const uint8_t cmd, const uint16_t method,
            const uint32_t file_pos, const uint32_t file_size,
            const uint16_t buffer_size,
            const uint8_t* buffer) {
            uint8_t* ptr = buffer_write_u8(tx_buf + sizeof(uint16_t), cmd);
            ptr = buffer_write_u16(ptr, method);
            ptr = buffer_write_u32(ptr, file_pos);
            ptr = buffer_write_u32(ptr, file_size);
            ptr = buffer_write_u16(ptr, buffer_size);
            ptr = buffer_write_u8vec(ptr, buffer, static_cast<size_t>(buffer_size));
            build_end(ptr);
        }

        // [size:2][cmd:1][method:2][file_pos:4][file_size:4][buffer_size:2][result:1][crc:4]
        // size
        void build_data_packet_rep(const uint8_t cmd, const uint16_t method,
            const uint32_t file_pos, const uint32_t file_size,
            const uint16_t buffer_size, const uint8_t result) {
            uint8_t* ptr = buffer_write_u8(tx_buf + sizeof(uint16_t), cmd);
            ptr = buffer_write_u16(ptr, method);
            ptr = buffer_write_u32(ptr, file_pos);
            ptr = buffer_write_u32(ptr, file_size);
            ptr = buffer_write_u16(ptr, buffer_size);
            ptr = buffer_write_u8(ptr, result);
            build_end(ptr);
        }

        // build end: compute crc and set tx size
        void build_end(uint8_t* ptr) {
            ptr = buffer_write_u32(ptr, 0xB4B3B2B1);  // crc
            tx_size = ptr - tx_buf;
            buffer_write_u16(tx_buf, static_cast<uint16_t>(tx_size));
        }

        uint8_t tx_buf[cfg::packet_buffer_size];
        size_t tx_size;
    };

    class packet_parser {
    public:
        // rx_buffer and rx_size are used directly to read data from socket
        uint8_t rx_buf[cfg::packet_buffer_size];
        size_t rx_size;

        void recv_data(const uint8_t* buffer, const size_t size) {
            std::memcpy(rx_buf, buffer, size);
            rx_size = size;
        }

    private:
        // header
        uint16_t size_;
        uint8_t cmd_;
        uint16_t method_;

        // crc
        uint32_t crc_;

        // result
        uint8_t result_;

        // send part
        uint32_t file_pos_;
        uint32_t file_size_;
        uint16_t buffer_size_;

        const uint8_t* buffer_;

    public:
        uint16_t size() { return size_; }
        uint8_t cmd() { return cmd_; }
        uint16_t method() { return method_; }
        uint32_t crc() { return crc_; }

        bool result_success() { return result_ == rep_result::success; }
        bool result_retry() { return result_ == rep_result::retry; }
        bool result_fail() { return result_ == rep_result::fail; }

        bool valid() { return valid_; }  // properly parsed and crc is valid

        uint32_t file_pos() { return file_pos_; }
        uint32_t file_size() { return file_size_; }
        uint16_t buffer_size() { return buffer_size_; }
        const uint8_t* buffer() { return buffer_; }

        void print() {
            const char* STR_CMD[] = {
                "UNKNOWN",        "REQ_EXEC_FUNC",   "REP_EXEC_FUNC",  "REQ_SEND_INPUT",
                "REP_SEND_INPUT", "REQ_SEND_OUTPUT", "REP_SEND_OUTPUT" };

            const char* STR_RESULT[] = { "SUCCESS", "RETRY", "FAIL", "UNKNOWN" };

            printf("> PACKET ==================================\n");
            printf("\nRaw:\n");
            for (size_t i = 0; i < 30 && i < rx_size; i++) {
                printf("%02X ", rx_buf[i]);
            }
            printf("\n\n");
            printf("Size:        %" PRIu16 "\n", size_);
            printf("Command:     %s\n", STR_CMD[cmd_ > 6 ? 0 : cmd_]);
            printf("Method:      %" PRIu16 "\n", method_);
            printf("CRC32:       0x%X\n", crc_);
            printf("Valid:       %s\n", valid_ ? "yes" : "no");

            if (cmd_ == command::REP_EXEC_FUNC || cmd_ == command::REQ_SEND_INPUT ||
                cmd_ == command::REP_SEND_OUTPUT) {
                printf("Result:      %s\n", STR_RESULT[result_ > 2 ? 3 : result_]);
            }

            if (cmd_ == command::REQ_SEND_INPUT || cmd_ == command::REQ_SEND_OUTPUT ||
                cmd_ == command::REP_SEND_INPUT || cmd_ == command::REP_SEND_OUTPUT) {
                printf("File Pos:    %" PRIu32 "\n", file_pos_);
                printf("File Size:   %" PRIu32 "\n", file_size_);
                printf("Buffer Size: %" PRIu16 "\n", buffer_size_);
            }

            if (cmd_ == command::REQ_SEND_INPUT || cmd_ == command::REQ_SEND_OUTPUT) {
                printf("Data:        ");
                for (size_t i = 0; i < 30 && i < buffer_size_; i++) {
                    printf("%02X ", buffer_[i]);
                }
            }

            printf("\n===========================================\n\n");
        }

        packet_parser() : rx_size(0), valid_(false) {
            std::memset(rx_buf, 0, cfg::packet_buffer_size);
            buffer_ = &rx_buf[15];
            reset();
        }

        void reset() {
            rx_size = 0;
            size_ = 0;
            cmd_ = 0;
            method_ = 0;
            crc_ = 0;
            result_ = 0;
            file_pos_ = 0;
            file_size_ = 0;
            buffer_size_ = 0;
            valid_ = false;
        }

        bool parse() {
            valid_ = false;
            crc_ = 0;

            if (rx_size < 9) {
                fprintf(stderr, "Error: packet_parser - rx buffer size is too small\n");
                return false;
            }

            uint8_t* ptr = rx_buf;
            size_ = bytes_to_u16(ptr);
            ptr += sizeof(uint16_t);

            if (size_ > rx_size || size_ > cfg::packet_buffer_size) {
                fprintf(stderr, "Error: packet_parser - rx buffer size is too big\n");
                return false;
            }

            cmd_ = *ptr;
            ptr += sizeof(uint8_t);

            method_ = bytes_to_u16(ptr);
            ptr += sizeof(uint16_t);

            if (cmd_ == command::REQ_EXEC_FUNC) {
                if (size_ != 9) {
                    fprintf(
                        stderr,
                        "Error: packet_parser - invalid packet size for REQ_EXEC_FUNC\n");
                    return false;
                }

            }
            else if (cmd_ == command::REP_EXEC_FUNC) {
                if (size_ != 10) {
                    fprintf(
                        stderr,
                        "Error: packet_parser - invalid packet size for REQ_EXEC_FUNC\n");
                    return false;
                }

                result_ = *ptr;
                ptr += sizeof(uint8_t);

            }
            else if (cmd_ == command::REQ_SEND_INPUT ||
                cmd_ == command::REQ_SEND_OUTPUT) {
                if (size_ < 19) {
                    fprintf(
                        stderr,
                        "Error: packet_parser - invalid packet size for REQ_SEND_INPUT\n");
                    return false;
                }
                file_pos_ = bytes_to_u32(ptr);
                ptr += sizeof(uint32_t);

                file_size_ = bytes_to_u32(ptr);
                ptr += sizeof(uint32_t);

                buffer_size_ = bytes_to_u16(ptr);
                ptr += sizeof(uint16_t);

                if (buffer_size_ > (size_ - 19)) {
                    fprintf(stderr,
                        "Error: packet_parser - invalid buffer size size for "
                        "REQ_SEND_INPUT\n");
                    return false;
                }

                ptr += static_cast<size_t>(buffer_size_);

            }
            else if (cmd_ == command::REP_SEND_INPUT ||
                cmd_ == command::REP_SEND_OUTPUT) {
                if (size_ < 20) {
                    fprintf(
                        stderr,
                        "Error: packet_parser - invalid packet size for REP_SEND_INPUT\n");
                    return false;
                }

                file_pos_ = bytes_to_u32(ptr);
                ptr += sizeof(uint32_t);

                file_size_ = bytes_to_u32(ptr);
                ptr += sizeof(uint32_t);

                buffer_size_ = bytes_to_u16(ptr);
                ptr += sizeof(uint16_t);

                result_ = *ptr;
                ptr += sizeof(uint8_t);

            }
            else {
                fprintf(stderr, "Error: packet_parser - invalid command\n");
                return false;
            }

            crc_ = bytes_to_u32(ptr);

            valid_ = crc_ == 0xB4B3B2B1;

            return valid_;
        }

    private:
        bool valid_;
    };

    // message serializers
    // -----------------------------------------------------------------------------

    class msg_serializer {
    public:
        virtual void serialize(uint8_t* buffer, size_t* buffer_size) = 0;
        virtual bool deserialize(uint8_t* buffer, const size_t buffer_size) = 0;
    };

    // Send and Receive fragmented data packets
    // -----------------------------------------------------------------------------

    bool rpc_send_msg(const uint16_t method, uint8_t cmd_req, const uint8_t cmd_rep,
        std::atomic<bool>& keep_running, socket_transceiver* conn,
        const SOCKET socket_id, packet_builder& tx, packet_parser& rx,
        rpc_frag& frag) {
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
                static_cast<uint16_t>(data_size),
                frag.buffer + file_pos);

            int tx_tries = 5;
            while (--tx_tries > 0) {
                if (cfg::print_msg_frag) printf("[rpc_send_msg] tx_tries %d\n", tx_tries);

                timer_perf t;
                t.tic();

                if (conn->send(socket_id, tx.buffer(), tx.size()) == RET_SEND_FAIL) {
                    fprintf(stderr,
                        "[rpc_send_msg] error: fail to SEND msg part: %zu/%zu\n",
                        file_pos, frag.buffer_size);
                    return false;
                }

                if (cfg::print_msg_frag)
                    printf("[rpc_send_msg] send time: %0.6f\n", t.toc());

                // rx.rx_size = cfg::recv_buffer_size;
                rx.rx_size = 20;

                t.tic();
                ret_recv res =
                    conn->try_recv(socket_id, rx.rx_buf, &rx.rx_size, cfg::timeout_recv);
                if (cfg::print_msg_frag)
                    printf("[rpc_send_msg] try recv: %0.6f\n", t.toc());

                if (res == RET_RECV_FAIL) {
                    fprintf(
                        stderr,
                        "[rpc_send_msg] error: fail to recv msg part (socket): %zu/%zu\n",
                        file_pos, frag.buffer_size);
                    return false;
                }
                else if (res == RET_RECV_TIMEOUT) {
                    fprintf(
                        stderr,
                        "[rpc_send_msg] error: fail to recv msg part (timeout): %zu/%zu\n",
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
                    }
                    else if (rx.result_retry()) {
                        printf("[rpc_send_msg] retry: tx_tries %d\n", tx_tries);
                        continue;
                    }
                    else {
                        fprintf(stderr,
                            "[rpc_send_msg] error: fail to send part (abort): %zu/%zu\n",
                            file_pos, frag.buffer_size);
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
        std::atomic<bool>& keep_running, socket_transceiver* conn,
        SOCKET socket_id, packet_builder& tx, packet_parser& rx,
        rpc_frag& frag) {
        int rx_tries = 5;
        frag.reset();

        while (--rx_tries > 0 && keep_running && conn->is_active()) {
            if (cfg::print_msg_frag) printf("[rpc_recv_msg] rx_tries %d\n", rx_tries);

            rx.rx_size = cfg::packet_buffer_size;

            ret_recv res =
                conn->try_recv(socket_id, rx.rx_buf, &rx.rx_size, cfg::timeout_recv);
            if (res == RET_RECV_FAIL) {
                fprintf(stderr,
                    "[rpc_recv_msg] error: fail to recv msg req (socket error)");
                return false;
            }
            else if (res == RET_RECV_TIMEOUT) {
                fprintf(stderr,
                    "[rpc_recv_msg] error: fail to recv msg part (timeout exceed)\n");
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
                fprintf(stderr, "[rpc_recv_msg] error: unexpected command (%u != %u)\n",
                    rx.cmd(), cmd_rep);
                result = rep_result::fail;
            }

            if (rx.method() != method) {
                fprintf(stderr, "[rpc_recv_msg] error: invalid method\n");
                result = rep_result::fail;
            }

            if (rx.file_size() > frag.max_size) {
                fprintf(stderr,
                    "[rpc_recv_msg] error: file size is too big(%" PRIu32 ") %zu\n",
                    rx.file_pos(), frag.max_size);
                result = rep_result::fail;
            }

            if (frag.buffer_size == 0 && rx.file_pos() > 0) {
                fprintf(stderr,
                    "[rpc_recv_msg] error: inital file_pos must be zero (%" PRIu32
                    ")\n",
                    rx.file_pos());
                result = rep_result::retry;
            }
            else if (frag.buffer_size + static_cast<size_t>(rx.buffer_size()) >
                frag.max_size) {
                fprintf(stderr,
                    "[rpc_recv_msg] error: invalid file_pos and max_size (%" PRIu32
                    ")\n",
                    rx.file_pos());
                result = rep_result::retry;
            }
            else if (frag.buffer_size + static_cast<size_t>(rx.buffer_size()) >
                rx.file_size()) {
                fprintf(stderr,
                    "[rpc_recv_msg] error: invalid file_pos and max_size (%" PRIu32
                    ")\n",
                    rx.file_pos());
                result = rep_result::retry;
            }

            if (result == rep_result::success) {
                std::memcpy(frag.buffer + frag.buffer_size, rx.buffer(),
                    static_cast<size_t>(rx.buffer_size()));
                frag.buffer_size += static_cast<size_t>(rx.buffer_size());
            }

            tx.build_data_packet_rep(cmd_rep, method, rx.file_pos(), rx.file_size(),
                rx.buffer_size(), result);
            conn->send(socket_id, tx.buffer(), tx.size());

            if (result == rep_result::fail) {
                return false;
            }

            if (static_cast<size_t>(rx.file_size()) == frag.buffer_size) {
                if (cfg::print_msg_frag)
                    printf("[rpc_recv_msg] recv finished! %d\n", rx_tries);
                break;
            }
            rx_tries = 5;
        }

        if (cfg::print_msg_frag) printf("[rpc_recv_msg] end\n");
        return true;
    }

    // RPC Server
    // -----------------------------------------------------------------------------

    template <typename input_message_type, typename output_message_type>
    class rpc_server {
    public:
        rpc_server(const char* address, const uint16_t port,
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

        void register_method(
            const uint16_t method,
            std::function<void(input_message_type*, output_message_type*)>
            callback) {
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
                    printf("waitning client....\n");
                    ret_accept res = conn_->try_accept(client, cfg::timeout_accept);
                    if (res == RET_ACCEPT_FAIL && keep_running_ == false) {
                        break;
                    }
                    else if (res == RET_ACCEPT_TIMEOUT) {
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
        void client_loop(const conn_info& client) {
            while (keep_running_ && conn_->is_active()) {
                rx_.rx_size = 9;

                ret_recv res =
                    conn_->try_recv(client, rx_.rx_buf, &rx_.rx_size, cfg::timeout_recv);

                input_message_type input_param;
                output_message_type output_param;

                if (res == RET_RECV_FAIL) {
                    fprintf(stderr, "[rpc_server] error: client disconnected\n");
                    break;
                }
                else if (res == RET_RECV_TIMEOUT) {
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

                if (rx_.cmd() == command::REQ_EXEC_FUNC &&
                    callbacks_.find(method) != callbacks_.end()) {
                    tx_.rep_exec_func(rx_.method(), rep_result::success);
                    if (conn_->send(client, tx_.buffer(), tx_.size()) == RET_SEND_FAIL) {
                        fprintf(stderr, "[rpc_server] error: fail to send REP_EXEC_FUNC\n");
                        break;
                    }

                    if (rpc_recv_msg(method, command::REQ_SEND_INPUT,
                        command::REP_SEND_INPUT, keep_running_, conn_,
                        client.socket_id, tx_, rx_, frag_)) {
                        reinterpret_cast<msg_serializer*>(&input_param)
                            ->deserialize(frag_.buffer, frag_.buffer_size);

                        callbacks_[method](&input_param, &output_param);

                        frag_.buffer_size = frag_.max_size;
                        reinterpret_cast<msg_serializer*>(&output_param)
                            ->serialize(frag_.buffer, &frag_.buffer_size);

                        rpc_send_msg(method, command::REQ_SEND_INPUT, command::REP_SEND_INPUT,
                            keep_running_, conn_, client.socket_id, tx_, rx_, frag_);
                    }
                }
                else {
                    fprintf(stderr, "[rpc_server] error: invalid request\n");
                    tx_.rep_exec_func(rx_.method(), rep_result::fail);
                    if (conn_->send(client, tx_.buffer(), tx_.size()) == RET_SEND_FAIL) {
                        break;
                    }
                }
            }
        }

        // class attributes

        std::unordered_map<uint16_t, std::function<void(input_message_type*,
            output_message_type*)>>
            callbacks_;
        std::atomic<bool> keep_running_;
        tcp_server* conn_;
        packet_builder tx_;
        packet_parser rx_;
        rpc_frag frag_;
    };

    // RPC client
    // -----------------------------------------------------------------------------

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

        bool execute(const uint16_t method, msg_serializer* input,
            msg_serializer* output) {
            if (!conn_->is_active()) {
                fprintf(stderr, "[rpc_client] connection is not active\n");
                return false;
            }

            if (!send_exec_func(method)) {
                return false;
            }

            input->serialize(frag_.buffer, &frag_.buffer_size);

            if (!rpc_send_msg(method, command::REQ_SEND_INPUT, command::REP_SEND_INPUT,
                keep_running_, conn_, 0, tx_, rx_, frag_)) {
                printf("[rpc_client] fail to send msg\n");
                return false;
            }

            if (!rpc_recv_msg(method, command::REQ_SEND_INPUT, command::REP_SEND_INPUT,
                keep_running_, conn_, 0, tx_, rx_, frag_)) {
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
                fprintf(stderr,
                    "[rpc_client] send_exec_func: unexpected command replied\n");
                return false;
            }

            if (rx_.method() != method) {
                fprintf(stderr,
                    "[rpc_client] send_exec_func: invalid method (%" PRIu16
                    " != %" PRIu16 ")replied\n",
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

    // RPC server single
    // -----------------------------------------------------------------------------

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

        void register_method(
            std::function<void(input_message_type*, output_message_type*)>
            callback) {
            callback_ = callback;
        }

        inline void stop() { keep_running_ = false; }

        void run() {
            keep_running_ = true;

            while (keep_running_) {
                if (!conn_->listen()) {
                    printf("[rpc_server_single] (method: %" PRIu16
                        ") trying to open socket again: %s:%" PRIu16 "\n",
                        method_, conn_->connection_info().address,
                        conn_->connection_info().port);
                    conn_->close();
                    break;
                }

                conn_info client;
                while (conn_->is_active() && keep_running_) {
                    ret_accept res = conn_->try_accept(client, cfg::timeout_accept);
                    if (res == RET_ACCEPT_FAIL && keep_running_ == false) {
                        break;
                    }
                    else if (res == RET_ACCEPT_TIMEOUT) {
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
                if (rpc_recv_msg(method_, command::REQ_SEND_INPUT,
                    command::REP_SEND_INPUT, keep_running_, conn_,
                    client.socket_id, tx_, rx_, frag_)) {
                    reinterpret_cast<msg_serializer*>(&input_param)
                        ->deserialize(frag_.buffer, frag_.buffer_size);

                    callback_(&input_param, &output_param);

                    frag_.buffer_size = frag_.max_size;
                    reinterpret_cast<msg_serializer*>(&output_param)
                        ->serialize(frag_.buffer, &frag_.buffer_size);

                    rpc_send_msg(method_, command::REQ_SEND_INPUT, command::REP_SEND_INPUT,
                        keep_running_, conn_, client.socket_id, tx_, rx_, frag_);
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

    // RPC client single
    // -----------------------------------------------------------------------------

    template <typename input_message_type, typename output_message_type>
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

        bool execute(input_message_type* input, output_message_type* output) {
            if (!conn_->is_active()) {
                fprintf(stderr, "[rpc_client_single] connection is not active\n");
                return false;
            }

            reinterpret_cast<msg_serializer*>(input)->serialize(frag_.buffer,
                &frag_.buffer_size);

            if (!rpc_send_msg(0x0000, command::REQ_SEND_INPUT, command::REP_SEND_INPUT,
                keep_running_, conn_, 0, tx_, rx_, frag_)) {
                printf("[rpc_client_single] fail to send msg\n");
                return false;
            }

            if (!rpc_recv_msg(0x0000, command::REQ_SEND_INPUT, command::REP_SEND_INPUT,
                keep_running_, conn_, 0, tx_, rx_, frag_)) {
                printf("[rpc_client_single] fail to recv msg\n");
                return false;
            }

            if (!reinterpret_cast<msg_serializer*>(output)->deserialize(
                frag_.buffer, frag_.buffer_size)) {
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

    // Subscriber
    // -----------------------------------------------------------------------------

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

        void register_callback(std::function<void(message_type*)> callback) {
            callback_ = callback;
        }

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
                    }
                    else if (res == RET_ACCEPT_TIMEOUT) {
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
                if (rpc_recv_msg(method_, command::REQ_SEND_INPUT,
                    command::REP_SEND_INPUT, keep_running_, conn_,
                    client.socket_id, tx_, rx_, frag_)) {
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

    // Publisher
    // -----------------------------------------------------------------------------

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

            reinterpret_cast<msg_serializer*>(input)->serialize(frag_.buffer,
                &frag_.buffer_size);

            if (!rpc_send_msg(0, command::REQ_SEND_INPUT, command::REP_SEND_INPUT,
                keep_running_, conn_, 0, tx_, rx_, frag_)) {
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

    };  // namespace dc

#endif  // DARC_RCP_SINGLE_HEADER_HPP__
