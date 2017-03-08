cmake_minimum_required (VERSION 2.6)
project (tcpproxy)

#find_package(OpenSSL REQUIRED)
#include_directories(. ${OPENSSL_INCLUDE_DIR})
include_directories(.)

add_executable(tcpproxy main.c)

target_link_libraries(tcpproxy ${OPENSSL_LIBRARIES})
