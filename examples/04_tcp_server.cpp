#include "darc-rpc.hpp"

int main(int argc, char **argv) {
  dc::tcp_server server("0.0.0.0", 31311);

  if (!server.listen()) {
    return 1;
  }

  while (server.is_active()) {
    dc::conn_info client;
    if (server.try_accept(client, 1000) != dc::RET_ACCEPT_SUCCESS) {
      printf("Waiting client to connect...\n");
      continue;
    }
    printf("New client connected: ");
    client.print();
    printf("\n");

    const size_t BUF_SIZE = 255;
    uint8_t buffer[BUF_SIZE];

    while (server.is_active()) {
      size_t buf_size = BUF_SIZE;
      dc::ret_recv res =
          server.try_recv(client.socket_id, buffer, &buf_size, 1000);

      if (res == dc::RET_RECV_SUCCESS) {
        printf("RX[0]: %02X\n", buffer[0]);
        buffer[0] += 1;

        int res = server.send(client.socket_id, buffer, 1);
        if (res == dc::RET_SEND_FAIL) {
          printf("Fail to send message\n");
          break;
        }
      } else if (res == dc::RET_RECV_FAIL) {
        printf("RET_RECV_FAIL\n");
        break;
      }
    }
  }

  server.close();

  return 0;
}
