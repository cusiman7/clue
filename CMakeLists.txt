cmake_minimum_required(VERSION 3.12.4)
project(cl CXX)
set(CMAKE_CXX_STANDARD 17)

include_directories(include)

function(add_example name)
add_executable(${name}
  examples/${name}.cpp
)
if(MSVC)
  target_compile_options(${name} PRIVATE /W4 /WX)
else()
  target_compile_options(${name} PRIVATE -Wall -Werror -Wpedantic)
endif()
endfunction()

add_example(simple)
add_example(features)
add_example(add)
add_example(vector)
add_example(user_types)
