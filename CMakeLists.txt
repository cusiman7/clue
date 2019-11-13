cmake_minimum_required(VERSION 3.13)
project(cl CXX)
set(CMAKE_CXX_STANDARD 17)

include_directories(include)

function(add_example name)
add_executable(${name}
  examples/${name}.cpp
)
target_compile_options(${name} PRIVATE -Wall -Werror -Wpedantic)
endfunction()

add_example(simple)
add_example(features)
add_example(add)
add_example(vector)
