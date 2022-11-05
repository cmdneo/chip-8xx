cmake_minimum_required(VERSION 3.10)
project(Chip-8xx DESCRIPTION "Chip-8 emulator and assembler")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_compile_options(-Wall -Wextra -Wnull-dereference -Wshadow -Wformat=2 -pedantic)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
include_directories("${CMAKE_SOURCE_DIR}/include")
# add_compile_options(-fsanitize=address,undefined)
# add_link_options(-fsanitize=address,undefined)

add_executable(c8emu "chip8/main.cxx" "chip8/emulator.cxx" "chip8/decoder.cxx")
target_link_libraries(c8emu raylib)

add_executable(c8asm "assembler/assembler.cxx")
target_link_libraries(c8asm)

