cmake_minimum_required(VERSION 3.31)

project(dev)

option(BUILD_TESTING "Build tests" ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(
    dev
    src/cache/cache.cpp
    src/packages/composer_manager.cpp
    main.cpp
)

target_include_directories(
    dev PRIVATE
    include
)
