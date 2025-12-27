cmake_minimum_required(VERSION 3.10)

project(Translator)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 包含头文件目录
include_directories(include)

# 查找源文件
file(GLOB SOURCES "src/*.cpp")

# 创建可执行文件
add_executable(translator ${SOURCES})

set_target_properties(translator PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}"
)

if(MSVC)
    target_compile_options(translator PRIVATE /W4 /utf-8)
else()
    target_compile_options(translator PRIVATE -Wall -Wextra -Wpedantic)
endif()