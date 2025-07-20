cmake_minimum_required(VERSION 3.31)

project(span VERSION 1.0.0 LANGUAGES CXX)

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

# Auto-discovery of package managers
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(REGISTRAR_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/cmake/generate_registrar.py)
set(GENERATED_REGISTRAR_FILE ${CMAKE_CURRENT_BINARY_DIR}/generated/registrar.cpp)
add_custom_command(
    OUTPUT ${GENERATED_REGISTRAR_FILE}
    COMMAND ${Python3_EXECUTABLE} ${REGISTRAR_SCRIPT} ${CMAKE_CURRENT_SOURCE_DIR} ${GENERATED_REGISTRAR_FILE}
    DEPENDS ${REGISTRAR_SCRIPT}
    COMMENT "Generating package manager registrar"
)
add_custom_target(
    GenerateManagerRegistrar ALL
    DEPENDS ${GENERATED_REGISTRAR_FILE}
)

# Main executable
add_executable(
    span
    src/cache.cpp
    src/packages/composer.cpp
    src/packages/manager.cpp
    src/packages/manager_factory.cpp
    src/thread_pool.cpp
    main.cpp
    ${GENERATED_REGISTRAR_FILE}
)

add_dependencies(span GenerateManagerRegistrar)

target_include_directories(
    span PRIVATE
    include
)

add_subdirectory(deps/simdjson)
add_subdirectory(deps/cli11)

target_link_libraries(
    span PRIVATE
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
