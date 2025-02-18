cmake_minimum_required(VERSION 3.31)

project(dev)

option(BUILD_TESTING "Build tests" ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(
    dev
    src/cache/cache.cpp
    src/packages/composer.cpp
    main.cpp
)

target_include_directories(
    dev PRIVATE
    include
)

add_subdirectory(deps/simdjson)
add_subdirectory(deps/cli11)

target_link_libraries(
    dev PRIVATE
    simdjson
    cli11
)
