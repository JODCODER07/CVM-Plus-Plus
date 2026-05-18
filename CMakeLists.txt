cmake_minimum_required(VERSION 3.10)
project(cvm++ VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include_directories(include)

add_executable(cvm++
    src/main.cpp
    src/chunk.cpp
    src/value.cpp
    src/lexer.cpp
    src/ast.cpp
    src/parser.cpp
    src/compiler.cpp
    src/vm.cpp
)

if(WIN32)
    target_link_libraries(cvm++ ws2_32)
endif()

