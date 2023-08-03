cmake_minimum_required(VERSION 3.21)
project(WebServer)

set(CMAKE_CXX_STANDARD 23)

add_executable(WebServer code/epoll_server_ET.cpp code/client.cpp code/selectserver.cpp code/epoll_server_LT.cpp code/kqueueServer.cpp)
