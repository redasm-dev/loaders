cmake_minimum_required(VERSION 3.25)

project(mz C)

if(NOT TARGET redasm::redasm)
    find_package(redasm REQUIRED)
endif()

add_library(${PROJECT_NAME} SHARED)

target_sources(${PROJECT_NAME} 
    PRIVATE
        pe/dirs/debug.c
        pe/dirs/exceptions.c
        pe/dirs/exports.c
        pe/dirs/imports.c
        pe/dirs/resources.c
        pe/format.c
        pe/pe.c
        common.c
        plugin.c
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(${PROJECT_NAME} PRIVATE redasm::redasm)
