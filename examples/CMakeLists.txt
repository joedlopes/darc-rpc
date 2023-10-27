cmake_minimum_required(VERSION 3.12)

project(darc-rpc)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(../include)

# example 1 - rpc multiple methods
add_executable(01_rpc_server    01_rpc_server.cpp)
add_executable(01_rpc_client    01_rpc_client.cpp)

# example 2 - rpc single method
add_executable(02_rpc_server_single    02_rpc_server_single.cpp)
add_executable(02_rpc_client_single    02_rpc_client_single.cpp)

# example 3 - rpc publisher and subscriber
add_executable(03_publisher     03_publisher.cpp)
add_executable(03_subscriber    03_subscriber.cpp)

# example 4 - rpc publisher and subscriber
add_executable(04_tcp_client    04_tcp_client.cpp)
add_executable(04_tcp_server    04_tcp_server.cpp)
