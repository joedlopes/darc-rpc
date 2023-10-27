#ifndef DARC_RPC_BUFFER_WRITE_HPP__
#define DARC_RPC_BUFFER_WRITE_HPP__

#include <chrono>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace dc {

uint32_t bytes_to_u32(const uint8_t* ptr) {
  return static_cast<uint32_t>(ptr[0]) | (static_cast<uint32_t>(ptr[1]) << 8) |
         (static_cast<uint32_t>(ptr[2]) << 16) | (static_cast<uint32_t>(ptr[3]) << 24);
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

uint8_t* buffer_write_u8vec(uint8_t* buffer, const uint8_t* data, const size_t size) {
  std::memcpy(buffer, data, size);
  return buffer + size;
}

class timer_perf {
 public:
  timer_perf() { tic(); }

  void tic() { start_time = std::chrono::high_resolution_clock::now(); }

  float toc() {
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);
    return duration.count() / 1e9f;  // Convert to seconds
  }

 private:
  std::chrono::high_resolution_clock::time_point start_time;
};

};  // namespace dc

#endif  // DARC_RPC_BUFFER_WRITE_HPP__