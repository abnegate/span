cmake_minimum_required(VERSION 3.31)

project(simdjson)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(
    simdjson
    src/simdjson.cpp
)

target_include_directories(
    simdjson PUBLIC
    include
)
