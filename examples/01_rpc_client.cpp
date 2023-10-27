#include "darc-rpc.hpp"
#include "msg_sample.hpp"

int main(int argc, char* argv[]) {
  const uint16_t M_SUM = 0x0000;
  const uint16_t M_SUM_SQUARED = 0x0001;

  dc::socket_requirements::init();
  dc::rpc_client client("0.0.0.0", 31311);

  if (!client.connect()) {
    return 1;
  }

  msg_params msg_in;
  msg_result msg_out;

  msg_in.a = 100;
  msg_in.b = 200;
  msg_in.c = 250;
  if (!client.execute(M_SUM, &msg_in, &msg_out)) {
    return 1;
  }
  printf("sum: %" PRIu16 "\n", msg_out.value);

  msg_in.a = 2;
  msg_in.b = 4;
  msg_in.c = 5;
  if (!client.execute(M_SUM_SQUARED, &msg_in, &msg_out)) {
    return 1;
  }
  printf("sum^2: %" PRIu16 "\n", msg_out.value);

  // connection will be closed automatically on the destructor of (rpc_client)

  return 0;
}
