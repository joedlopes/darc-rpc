#include "darc-rpc.hpp"
#include "msg_sample.hpp"

int main(int argc, char* argv[]) {
  dc::socket_requirements::init();
  dc::publisher<msg_params> pub("0.0.0.0", 31311);

  if (!pub.connect()) {
    return -1;
  }

  msg_params msg;
  msg.a = 1;
  msg.b = 2;
  msg.c = 3;

  pub.publish(&msg);

  return 0;
}
