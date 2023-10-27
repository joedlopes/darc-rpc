#include "darc-rpc.hpp"

int main(int argc, char** argv) {
  dc::tcp_client client("0.0.0.0", 31311);

  if (!client.connect()) {
    return 1;
  }

  const size_t BUF_SIZE = 255;

  std::vector<uint8_t> buffer;
  for (size_t i = 0; i < BUF_SIZE; i++) {
    buffer.push_back(i % 255);
  }

  int i = 0;
  while (client.is_active()) {
    size_t k = 0;

    for (k = 0; k < (1024 * 1024 / BUF_SIZE); k++) {
      if (client.send(buffer.data(), buffer.size()) == dc::RET_SEND_SUCCESS) {
        size_t buf_size = BUF_SIZE;
        dc::ret_recv res = client.try_recv(buffer.data(), &buf_size, 1000);
        if (res != dc::RET_RECV_SUCCESS) {
          break;
        }
        printf("rx[0]: %02X\n", buffer[0]);
      }
    }
  }

  client.close();

  return 0;
}
