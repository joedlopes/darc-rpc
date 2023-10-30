cmake_minimum_required(VERSION 3.0)
project(cvimage_example)


set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV 4 REQUIRED)

include_directories(../../include) # darc-rpc folder

include_directories(${OpenCV_INCLUDE_DIRS})


add_executable(test_msg_serialization test_msg_serialization.cpp)
target_link_libraries(test_msg_serialization ${OpenCV_LIBS})

add_executable(image_server image_server.cpp)
target_link_libraries(image_server ${OpenCV_LIBS})

add_executable(image_client image_client.cpp)
target_link_libraries(image_client ${OpenCV_LIBS})