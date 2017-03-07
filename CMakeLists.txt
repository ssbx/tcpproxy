cmake_minimum_required (VERSION 2.6)
project (tcpproxy)

#find_package(OpenSSL REQUIRED)
#include_directories(. ${OPENSSL_INCLUDE_DIR})
include_directories(.)

configure_file(
    ${CMAKE_SOURCE_DIR}/Makefile.in ${CMAKE_SOURCE_DIR}/Makefile)

add_executable(tcpproxy
    main.c)

target_link_libraries(tcpproxy ${OPENSSL_LIBRARIES})
