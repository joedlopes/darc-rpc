#ifndef DARC_RPC_RPC_PROTOCOL_HPP__
#define DARC_RPC_RPC_PROTOCOL_HPP__

#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "darc-rpc/config.hpp"
#include "darc-rpc/utils.hpp"

namespace dc {

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
  packet_builder() : tx_size(0) { std::memset(tx_buf, 0, cfg::packet_buffer_size); }

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
    uint8_t* ptr = buffer_write_u8(tx_buf + sizeof(uint16_t), command::REQ_EXEC_FUNC);
    ptr = buffer_write_u16(ptr, method);
    build_end(ptr);
  }

  // [size:2][cmd:1][method:2][result:1][crc:4]
  //    10
  void rep_exec_func(const uint16_t method, const uint8_t result) {
    uint8_t* ptr = buffer_write_u8(tx_buf + sizeof(uint16_t), command::REP_EXEC_FUNC);
    ptr = buffer_write_u16(ptr, method);
    ptr = buffer_write_u8(ptr, result);
    build_end(ptr);
  }

  void req_send_input(const uint16_t method, const uint32_t file_pos, const uint32_t file_size,
                      const uint16_t buffer_size, const uint8_t* buffer) {
    build_data_packet_req(command::REQ_SEND_INPUT, method, file_pos, file_size, buffer_size,
                          buffer);
  }

  void rep_send_input(const uint16_t method, const uint32_t file_pos, const uint32_t file_size,
                      const size_t buffer_size, const uint8_t result) {
    build_data_packet_rep(command::REP_SEND_INPUT, method, file_pos, file_size,
                          static_cast<uint16_t>(buffer_size), result);
  }

  void req_send_output(const uint16_t method, const uint32_t file_pos, const uint32_t file_size,
                       const uint16_t buffer_size, const uint8_t* buffer) {
    build_data_packet_req(command::REQ_SEND_OUTPUT, method, file_pos, file_size,
                          static_cast<uint16_t>(buffer_size), buffer);
  }

  void rep_send_output(const uint16_t method, const uint32_t file_pos, const uint32_t file_size,
                       const size_t buffer_size, const uint8_t result) {
    build_data_packet_rep(command::REP_SEND_OUTPUT, method, file_pos, file_size,
                          static_cast<uint16_t>(buffer_size), result);
  }

  // [size:2][cmd:1][method:2][file_pos:4][file_size:4][buffer_size:2][data:1][crc:4]
  // size
  void build_data_packet_req(const uint8_t cmd, const uint16_t method, const uint32_t file_pos,
                             const uint32_t file_size, const uint16_t buffer_size,
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
  void build_data_packet_rep(const uint8_t cmd, const uint16_t method, const uint32_t file_pos,
                             const uint32_t file_size, const uint16_t buffer_size,
                             const uint8_t result) {
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
    const char* STR_CMD[] = {"UNKNOWN",        "REQ_EXEC_FUNC",  "REP_EXEC_FUNC",
                             "REQ_SEND_INPUT", "REP_SEND_INPUT", "REQ_SEND_OUTPUT",
                             "REP_SEND_OUTPUT"};

    const char* STR_RESULT[] = {"SUCCESS", "RETRY", "FAIL", "UNKNOWN"};

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
        fprintf(stderr, "Error: packet_parser - invalid packet size for REQ_EXEC_FUNC\n");
        return false;
      }

    } else if (cmd_ == command::REP_EXEC_FUNC) {
      if (size_ != 10) {
        fprintf(stderr, "Error: packet_parser - invalid packet size for REQ_EXEC_FUNC\n");
        return false;
      }

      result_ = *ptr;
      ptr += sizeof(uint8_t);

    } else if (cmd_ == command::REQ_SEND_INPUT || cmd_ == command::REQ_SEND_OUTPUT) {
      if (size_ < 19) {
        fprintf(stderr, "Error: packet_parser - invalid packet size for REQ_SEND_INPUT\n");
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

    } else if (cmd_ == command::REP_SEND_INPUT || cmd_ == command::REP_SEND_OUTPUT) {
      if (size_ < 20) {
        fprintf(stderr, "Error: packet_parser - invalid packet size for REP_SEND_INPUT\n");
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

    } else {
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

}  // namespace dc

#endif  // DARC_RPC_RPC_PROTOCOL_HPP__
