cmake_minimum_required(VERSION 3.20)
project(Chip-8xx DESCRIPTION "Chip-8 emulator and assembler")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_compile_options(-Wall -Wextra -Wnull-dereference -Wshadow -Wformat=2 -Wpedantic)
add_compile_options("$<$<CONFIG:Debug>:-fsanitize=address,undefined>")
add_link_options("$<$<CONFIG:Debug>:-fsanitize=address,undefined>")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
include_directories("${CMAKE_SOURCE_DIR}/include")

add_executable(c8emu
    "emulator/emulator.cxx"
    "emulator/decoder.cxx"
    "emulator/main.cxx"
)
target_link_libraries(c8emu raylib m)

add_executable(c8asm 
    "assembler/lexer.cxx"
    "assembler/parser.cxx"
    "assembler/main.cxx"
)
target_link_libraries(c8asm)

