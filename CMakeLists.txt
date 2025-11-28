cmake_minimum_required(VERSION 3.22)
project(Server CXX)
set(CMAKE_CXX_STANDARD 20)

set(Boost_USE_STATIC_LIBS ON)

find_package(Boost 1.84 REQUIRED COMPONENTS log log_setup)

file(GLOB server_src "udptcp_server/*.cpp" "udptcp_server/server/*.cpp")
file(GLOB logging_src "udptcp_server/logging/*.cpp")

add_executable(Server ${server_src} ${logging_src})
target_link_libraries(Server PRIVATE Boost::log Boost::log_setup)