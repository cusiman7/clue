cmake_minimum_required(VERSION 3.13)
project(cl CXX)
set(CMAKE_CXX_STANDARD 17)

include_directories(include)

add_executable(simple
  examples/simple.cpp
)
target_compile_options(simple PRIVATE -Wall -Werror -Wpedantic)

add_executable(features
  examples/features.cpp
)
target_compile_options(features PRIVATE -Wall -Werror -Wpedantic)

add_executable(add
  examples/add.cpp
)
target_compile_options(add PRIVATE -Wall -Werror -Wpedantic)
