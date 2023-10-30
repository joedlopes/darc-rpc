# darc-rpc (d'Arc RPC) :bow_and_arrow:

Simple, lightweight, cross-platform, header-only, single header RPC written in C++ 11 and TCP sockets.

The main goal of this project is to provide an introduction about network communication, especially TCP sockets, by implementing a simple remote procedure call (RPC) library for cross-platform (Linux :penguin:, macOS, Windows).

Besides this educational purpose, it can be used as interface to communicate with constrainted hardware (embedded systems: Raspyberry Pi, low-power CPUs, and MPSoC FPGA), where installing a big RPC library is not ideal.

Another point is to provide a model like :robot: ROS nodes communication, I love ROS2 :revolving_hearts:, but sometimes I just want a simple publisher/subscriber without installing an entire distribution.

## Usage

You can just copy the single header (darc-rcp.hpp) to your project or either use the headers separetely. In the following examples, I use the single header:

### Define a Serializable message

At first, we need to implement an interface to serialize and deserialize buffer to be transfered in via socket communcation. 

msg_sample.hpp:
```cpp
#include "darc-rpc.hpp"

class msg_params : public dc::msg_serializer {
 public:
  uint8_t a, b, c;

  virtual void serialize(uint8_t* buffer, size_t* buffer_size) override {
    buffer[0] = a;
    buffer[1] = b;
    buffer[2] = c;

    *buffer_size = 3;
  }

  virtual bool deserialize(uint8_t* buffer, const size_t buffer_size) override {
    if (buffer_size < 3) {
      return false;
    }
    a = buffer[0];
    b = buffer[1];
    c = buffer[2];
    return true;
  }
};

class msg_result : public dc::msg_serializer {
 public:
  uint16_t value;

  virtual void serialize(uint8_t* buffer, size_t* buffer_size) override {
    buffer[0] = static_cast<uint8_t>(value & 0x00FF);
    buffer[1] = static_cast<uint8_t>(value >> 8);
    *buffer_size = 2;
  }

  virtual bool deserialize(uint8_t* buffer, const size_t buffer_size) override {
    if (buffer_size != 2) {
      return false;
    }
    value = static_cast<uint16_t>(buffer[0]) |
            (static_cast<uint16_t>(buffer[1]) << 8);
    return true;
  }
};
```

### Example of RCP client & server with multiple methods

rcp_server.cpp:
```cpp
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
```

rpc_client.cpp:
```cpp
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
```

### Example RCP client & server single method

If you have only one method, you can use the dc::rpc_server_single for server:

```cpp
dc::rpc_server_single<msg_params, msg_result> server("0.0.0.0", 31311);

server.register_method([](msg_params* input, msg_result* output) {
    output->value = input->a + input->b + input->c;
});

server.run();
```

and dc::rpc_client_single for client:

```cpp
dc::rpc_client_single<msg_params, msg_result> client("0.0.0.0", 31311);
client.connect();

msg_params msg_in;
msg_results msg_out;

msg_in.a = 100; 
msg_in.b = 200; 
msg_in.c = 250;
client.execute(&msg_in, &msg_out);
```

### Publish and Subscriber

It is also possible to make a publisher and subscriber architecture:

sample_subscriber.cpp:
```cpp
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
```

sample_publisher.cpp:
```cpp
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
```

### TCP Sockets

Non-blocking and blocking modes are avaiable in the dc::tcp_server and dc::tcp_client:

Example of TCP Server (non-blocking read with timout):
```cpp
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
```

Example of TCP Client (non-blocking read):
```cpp
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
```

### Example for remote image processing or large files

[01. Image Processing RPC Server and RCP Client](examples/remote_image_processing/README.md)



## Configuration / Defines

Before loading "darc-rpc.hpp", you can tune the data sizes according to your project and desired performance.

If you want to transmit data between process in the same machine, you can increase *recv_buffer_size* and *packet_buffer_size* to 60K.
It will allow you allow to transfer large data (HD Images) in real-time. However, it will increase the RAM usage.

If you want to optimize the performance for low RAM use, you can set the *recv_buffer_size* to 256.

Timeouts: you can increase or decrease the timeouts for reception.

As default the *TCP_NODELAY* flag is enabled, in Linux (ubuntu) it made the transmission significantly faster. If you prefre, you can disable it on tcp_client and tcp_server classes.

```cpp

#include <cstddef>

namespace dc { namespace cfg {

// Print debug messages
const bool print_msg_frag = false;

// Print Received packets
const bool print_rx_packets = false;

// Timeout for waiting message
const int timeout_recv = 1000;

// Timeout for accepting client connection
const int timeout_accept = 1000;

// Buffer size for reading and writing (tune for speed)
// For the same machine, it is possible to set it to 60k
const size_t recv_buffer_size = 1400;

// Packet buffer size
constexpr size_t packet_buffer_size = recv_buffer_size;

// Message serialization -> Maximum buffer size
constexpr size_t msg_buffer_size = 3000 * 3000 * 20;

// Socket buffer size for read and writing
constexpr int socket_buffer_sizes = msg_buffer_size;

}; };  //cfg // namespace dc

#define DARC_RPC_CFG

#include "darc-rpc.hpp"

int main() ...
```

## Build Examples

You can build the examples using CMake:

```bash
mkdir -p ./examples/build
cd ./examples/build
cmake ..
make
```

Or either build with make using the Makefile:

```bash
cd ./examples/
make
```

## Limitations

For simplification, the current implementation is not multi-thread. 
Therefore, 
- the server/subscriber do not support multiple clients 
- the server is not prepared to execute multiple methods simultaneously

## TODO / Roadmap

To be done in the future:

- [x] Example: remote image processing
- [ ] Python Implementation
- [ ] C# Implementation (for Unity 3D)
- [ ] Example: multi-node multi-process system with publisher and subscriber
- [ ] Support UDP sockets
- [ ] Support Shared Memory
- [ ] Multi-node application
- [ ] Example: audio streaming
- [ ] Documentation: Protocol Format, Sequence Diagram
- [ ] rpc_server: support methods with different input and output messages
- [ ] Server: multithreading to handle multiple clients

## d'Arc Framework

*d'Arc*  is the current **pre**fix for my [frameworks](https://github.com/darc-framework) :)

## Citation

If this repository helped you in some research or publication, it would be nice to have a citation, although not necessary.

```bibtex
@misc{jojo2023,
  author = {da Silva, Joed Lopes},
  title = {darc-rpc: D'Arc Framework RPC},
  year = {2023},
  howpublished = {\url{https://github.com/joedlopes/darc-rpc}},
  note = {Accessed: 28.10.2023}
}
```

## MIT

```
MIT License

Copyright (c) 2023 Joed Lopes da Silva

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```
