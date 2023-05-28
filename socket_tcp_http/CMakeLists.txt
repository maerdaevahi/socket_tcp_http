cmake_minimum_required(VERSION 3.16)
project(socket_tcp_http)

set(CMAKE_CXX_STANDARD 14)
add_definitions(-DUSE_CONCURRENCY -DDEBUG)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src SRC)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
add_link_options(-pthread)


message(STATUS ${SRC})
add_executable(socket_tcp_http ${SRC})
target_link_libraries(socket_tcp_http jsoncpp)
