#include "darc-rpc.hpp"
#include "msg_sample.hpp"

int main(int argc, char* argv[]) {
  dc::socket_requirements::init();

  dc::rpc_server_single<msg_params, msg_result> server("0.0.0.0", 31311);

  server.register_method([](msg_params* input, msg_result* output) {
    output->value = input->a + input->b + input->c;
  });

  server.run();

  return 0;
}
