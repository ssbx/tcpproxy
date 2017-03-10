cmake_minimum_required (VERSION 2.6)
project (hpx)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(Libevent REQUIRED)
find_package(OpenSSL REQUIRED)
add_subdirectory(deps/nghttp2)

include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}/deps/nghttp2/lib/includes/
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/nghttp2/lib/includes/
    ${OPENSSL_INCLUDE_DIR}
    ${LIBEVENT_INCLUDE_DIRS})


add_executable(hpx-server libevent-server.c http_parser.c http_parser.h)
add_executable(hpx-client libevent-client.c http_parser.c http_parser.h)

target_link_libraries(hpx-server ${LIBEVENT_LIBRARIES} ${OPENSSL_LIBRARIES} nghttp2 event event_openssl)
target_link_libraries(hpx-client ${LIBEVENT_LIBRARIES} ${OPENSSL_LIBRARIES} nghttp2 event event_openssl)
