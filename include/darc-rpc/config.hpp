#ifndef DARC_RPC_CONFIG
#define DARC_RPC_CONFIG

#include <cstddef>

namespace dc {

namespace cfg {

// Print debug messages
const bool print_msg_frag = false;

// Print Received packets
const bool print_rx_packets = false;

// Timeout for waiting message
const int timeout_recv = 1000;

// Timeout for accepting client connection
const int timeout_accept = 1000;

// Buffer size for reading and writing (tune for speed)
// For the same machine, it is possible to set it to 60k
const size_t recv_buffer_size = 1400;

// Packet buffer size
constexpr size_t packet_buffer_size = recv_buffer_size;

// Message serialization -> Maximum buffer size
constexpr size_t msg_buffer_size = 3000 * 3000 * 20;

// Socket buffer size for read and writing
constexpr int socket_buffer_sizes = msg_buffer_size * 10;

};  // namespace cfg
};  // namespace dc

#endif  // DARC_RPC_CONFIG
