cmake_minimum_required (VERSION 3.15)

project(Demo)

find_package(Threads REQUIRED)

aux_source_directory(. DIR_SRCS)

add_executable(server ${DIR_SRCS})

target_link_libraries(server Threads::Threads)