#include <opencv2/opencv.hpp>

#ifndef DARC_RPC_CONFIG
#define DARC_RPC_CONFIG

namespace cfg {

// Debug rx packets
const bool print_msg_frag = false;

const bool print_rx_packets = false;

const int timeout_recv = 1000;

const int timeout_accept = 1000;

// same machine we can use 60000, remote 1400
const size_t recv_buffer_size = 60000;

constexpr size_t packet_buffer_size = recv_buffer_size - 22;

constexpr size_t msg_buffer_size = 1024 * 1024 * 4;

constexpr int socket_buffer_sizes = msg_buffer_size;

};  // namespace cfg

#endif  // DARC_RPC_CONFIG

#include "darc-rpc.hpp"

class msg_cvimage : public dc::msg_serializer {
 public:
  cv::Mat img;
  bool encode = true;

  virtual void serialize(uint8_t* buffer, size_t* buffer_size) override {
    if (img.empty()) {
      buffer[0] = flag_empty;  // empty image
      *buffer_size = 1;
      return;
    }

    uint8_t* ptr = buffer;
    if (encode) {
      ptr = dc::buffer_write_u8(ptr, flag_encoded);
      std::vector<uchar> img_data;
      cv::imencode(".jpeg", img, img_data);
      ptr = dc::buffer_write_u8vec(
          ptr, reinterpret_cast<uint8_t*>(img_data.data()), img_data.size());

    } else {
      ptr = dc::buffer_write_u8(ptr, flag_raw);
      ptr = dc::buffer_write_u16(ptr, static_cast<uint16_t>(img.rows));
      ptr = dc::buffer_write_u16(ptr, static_cast<uint16_t>(img.cols));
      ptr = dc::buffer_write_u16(ptr, static_cast<uint16_t>(img.type()));
      ptr = dc::buffer_write_u8vec(ptr,
                                   reinterpret_cast<const uint8_t*>(img.data),
                                   img.elemSize() * img.total());
    }

    *buffer_size = ptr - buffer;
  }

  virtual bool deserialize(uint8_t* buffer, const size_t buffer_size) override {
    if (buffer_size < 1) {
      return false;
    }

    if (buffer[0] == flag_empty) {  // empty image
      img.release();
      return true;
    } else if (buffer[0] == flag_encoded) {  // encoded image
      buffer++;
      std::vector<uchar> enc_data(reinterpret_cast<uchar*>(buffer),
                                  buffer + buffer_size - 1);
      img = cv::imdecode(enc_data, cv::IMREAD_COLOR);
      encode = true;
    } else if (buffer[0] == flag_raw) {  // raw image
      encode = false;
      buffer++;
      int rows = static_cast<int>(dc::bytes_to_u16(buffer));
      buffer += 2;
      int cols = static_cast<int>(dc::bytes_to_u16(buffer));
      buffer += 2;
      int type = static_cast<int>(dc::bytes_to_u16(buffer));
      buffer += 2;
      cv::Mat(rows, cols, type, buffer).copyTo(img);  // force to copy the image
    }

    return true;
  }

 private:
  static const uint8_t flag_empty = 0x00;
  static const uint8_t flag_encoded = 0x01;
  static const uint8_t flag_raw = 0x02;
};
