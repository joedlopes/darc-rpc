#include "darc-rpc.hpp"
#include "msg_sample.hpp"

int main(int argc, char* argv[]) {
  dc::socket_requirements::init();
  dc::subscriber<msg_params> sub("0.0.0.0", 31311);

  sub.register_callback([](msg_params* msg) {
    printf("a = %02X, b = %02X, c = %02X\n", msg->a, msg->b, msg->c);
  });

  sub.run();

  return 0;
}
