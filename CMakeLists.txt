cmake_minimum_required(VERSION 3.13)
project(cl CXX)
set(CMAKE_CXX_STANDARD 17)

include_directories(include)
add_executable(features
  include/cl/cl.h
  examples/features.cpp
)
target_compile_options(features PRIVATE -Wall -Werror -Wpedantic)
