#include "darc-rpc.hpp"

int main(int argc, char** argv) {
  dc::tcp_client client("0.0.0.0", 31311);

  if (!client.connect()) {
    return 1;
  }

  const size_t BUF_SIZE = 255;
  uint8_t buffer[BUF_SIZE];

  for (size_t i = 0; i < BUF_SIZE; i++) {
    buffer[i] = (i % 255);
  }

  // send 100 messages
  for (int i = 0; i < 100 && client.is_active(); i++) {
    if (client.send(buffer, BUF_SIZE) == dc::RET_SEND_SUCCESS) {
      size_t buf_size = BUF_SIZE;
      dc::ret_recv res = client.try_recv(buffer, &buf_size, 1000);
      if (res != dc::RET_RECV_SUCCESS) {
        break;
      }
      printf("rx[0]: %02X\n", buffer[0]);
    }
  }

  client.close();

  return 0;
}
