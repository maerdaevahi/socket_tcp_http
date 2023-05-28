cmake_minimum_required(VERSION 3.16)
project(test_event)

set(CMAKE_CXX_STANDARD 14)
add_definitions(-DDEBUG)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src SRC)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
message(STATUS ${SRC})

add_executable(test_event ${SRC})
target_link_libraries(test_event event)
