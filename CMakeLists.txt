cmake_minimum_required(VERSION 3.13)
project(cl CXX)
set(CMAKE_CXX_STANDARD 17)

add_executable(cl
  cl.cpp
)
target_compile_options(cl PRIVATE -Wall -Werror -Wpedantic)
