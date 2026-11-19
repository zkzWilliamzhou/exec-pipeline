cmake_minimum_required(VERSION 3.20)
project(exec-pipeline VERSION 1.0.0 LANGUAGES CXX)

# C++26 with execution support
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags for C++26
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-std=c++2c)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-std=c++2c)
endif()

# Header-only library
add_library(exec-pipeline INTERFACE)
target_include_directories(exec-pipeline INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Test executable
add_executable(test_pipeline tests/test_pipeline.cpp)
target_link_libraries(test_pipeline PRIVATE exec-pipeline)

# Enable testing
enable_testing()
add_test(NAME ExecPipelineTests COMMAND test_pipeline)

# Compiler warnings
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Installation
install(TARGETS exec-pipeline
    EXPORT exec-pipeline-targets
    INCLUDES DESTINATION include
)

install(DIRECTORY include/
    DESTINATION include
)