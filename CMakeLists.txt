cmake_minimum_required (VERSION 3.15)

project(Demo)

find_package(Threads REQUIRED)

aux_source_directory(. DIR_SRCS)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer")

add_executable(server ${DIR_SRCS})

target_link_libraries(server Threads::Threads)