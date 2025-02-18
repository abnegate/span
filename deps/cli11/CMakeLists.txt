cmake_minimum_required(VERSION 3.31)

project(cli11)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(
    cli11 INTERFACE
)

target_include_directories(
    cli11 INTERFACE
    include
)
