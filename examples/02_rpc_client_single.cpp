#include "darc-rpc.hpp"
#include "msg_sample.hpp"

int main(int argc, char* argv[]) {
  dc::socket_requirements::init();
  dc::rpc_client_single<msg_params, msg_result> client("0.0.0.0", 31311);

  if (!client.connect()) {
    return 1;
  }

  msg_params msg_in;
  msg_result msg_out;

  msg_in.a = 100;
  msg_in.b = 200;
  msg_in.c = 250;
  if (!client.execute(&msg_in, &msg_out)) {
    return 1;
  }
  printf("sum: %" PRIu16 "\n", msg_out.value);

  return 0;
}
