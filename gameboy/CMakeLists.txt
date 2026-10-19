cmake_minimum_required(VERSION 3.20)

project(gameboy C)

if(NOT TARGET redasm::redasm)
    find_package(redasm REQUIRED)
endif()

add_library(${PROJECT_NAME} SHARED)

target_sources(${PROJECT_NAME} 
    PRIVATE
        gba/gba.c
        module.c
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(${PROJECT_NAME} PRIVATE redasm::redasm)

