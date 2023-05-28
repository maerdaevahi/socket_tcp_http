cmake_minimum_required(VERSION 3.16)
project(test_jsoncpp)

set(CMAKE_CXX_STANDARD 14)
add_definitions(-DDEBUG)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src SRC)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
link_libraries(jsoncpp)
message(STATUS ${SRC})

add_executable(test_jsoncpp ${SRC})
