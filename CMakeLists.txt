cmake_minimum_required(VERSION 3.25)
project(redasm-loaders C)

if(NOT TARGET redasm::redasm)
    find_package(redasm REQUIRED)
endif()

add_subdirectory(elf)
add_subdirectory(mz)
add_subdirectory(psx)
