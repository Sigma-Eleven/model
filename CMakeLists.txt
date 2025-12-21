cmake_minimum_required(VERSION 3.10)

project(WolfTranslator)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

add_executable(wolf_translator
    src/main.cpp
    src/lexer.cpp
    src/parser.cpp
    src/interpreter.cpp
    src/py_binding.cpp
)

set_target_properties(wolf_translator PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(MSVC)
    target_compile_options(wolf_translator PRIVATE /W4)
else()
    target_compile_options(wolf_translator PRIVATE -Wall -Wextra -Wpedantic)
endif()