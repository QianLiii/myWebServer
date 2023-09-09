cmake_minimum_required (VERSION 3.15)

project(Demo)

aux_source_directory(. DIR_SRCS)

add_executable(server ${DIR_SRCS})