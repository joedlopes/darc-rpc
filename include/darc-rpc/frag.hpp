#pragma once
#ifndef RPC_RPC_FRAG_HPP__
#define RPC_RPC_FRAG_HPP__

#include <cstddef>
#include <cstdint>
#include <cstring>

/**
 * Framentation of packets
 * Messages should be (de)serialized using the pre-allocated *buffer*
 */
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

#endif
