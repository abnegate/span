cmake_minimum_required(VERSION 3.31)

project(dev VERSION 1.0.0 LANGUAGES CXX)

option(BUILD_TESTING "Build tests" ON)
option(ENABLE_SANITIZERS "Enable address and undefined behavior sanitizers" OFF)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add compile warnings
if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Werror -Wpedantic)
endif()

# Enable sanitizers if requested
if(ENABLE_SANITIZERS)
    if(MSVC)
        add_compile_options(/fsanitize=address)
    else()
        add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address,undefined)
    endif()
endif()

# Main executable
add_executable(
    dev
    src/cache.cpp
    src/packages/composer.cpp
    src/packages/manager.cpp
    src/thread_pool.cpp
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
    ${CMAKE_THREAD_LIBS_INIT}
)

# Find threads for the thread pool
find_package(Threads REQUIRED)

if(BUILD_TESTING)
    enable_testing()
#    add_subdirectory(tests)
endif()
