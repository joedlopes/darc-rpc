#include "darc-rpc.hpp"
#include "msg_sample.hpp"

int main(int argc, char* argv[]) {
  // Windows-> WinSock Initialization
  dc::socket_requirements::init();

  // create rpc_server with message types
  dc::rpc_server<msg_params, msg_result> server("0.0.0.0", 31311);

  // register method
  const uint16_t M_SUM = 0x0000;
  server.register_method(M_SUM, [](msg_params* input, msg_result* output) {
    output->value = static_cast<uint16_t>(input->a) +
                    static_cast<uint16_t>(input->b) +
                    static_cast<uint16_t>(input->c);
  });

  // register another method
  const uint16_t M_SUM_SQUARED = 0x0001;
  server.register_method(M_SUM_SQUARED,
                         [](msg_params* input, msg_result* output) {
                           output->value = static_cast<uint16_t>(input->a) +
                                           static_cast<uint16_t>(input->b) +
                                           static_cast<uint16_t>(input->c);
                           output->value = output->value * output->value;
                         });

  // create TPC server, listen, accept client, and process requests
  server.run();

  // connection will be closed automatically on the destructor of (rpc_server)

  return 0;
}
