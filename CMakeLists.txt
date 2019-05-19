cmake_minimum_required(VERSION 3.13)
project(time-server C)

set(CMAKE_C_STANDARD 99)

include_directories(
        include
)

add_executable(time-server server/main.c server/thpool.c)

target_link_libraries(time-server pthread)
