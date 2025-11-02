cmake_minimum_required(VERSION 3.10)

project(air_control_c)

set(CMAKE_C_STANDARD 11)

add_executable(${CMAKE_PROJECT_NAME}
   src/main.c
   src/functions.c)

target_link_libraries(${CMAKE_PROJECT_NAME}
   m)