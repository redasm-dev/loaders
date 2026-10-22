cmake_minimum_required(VERSION 3.20)

project(mz C)

if(NOT TARGET redasm::redasm)
    find_package(redasm REQUIRED)
endif()

add_library(${PROJECT_NAME} SHARED)

target_sources(${PROJECT_NAME} 
    PRIVATE
        com/com.c
        common/common.c
        common/dos.c
        common/win16_ordinals.c
        le/exports.c
        le/fixup.c
        le/format.c
        le/imports.c
        le/le.c
        le/objects.c
        le/pages.c
        le/vxd.c
        mz/mz.c
        ne/entries.c
        ne/exports.c
        ne/format.c
        ne/modules.c
        ne/ne.c
        ne/relocs.c
        pe/dirs/config.c
        pe/dirs/debug.c
        pe/dirs/dotnet.c
        pe/dirs/exceptions.c
        pe/dirs/exports.c
        pe/dirs/imports.c
        pe/dirs/resources.c
        pe/dirs/tls.c
        pe/classifier.c
        pe/format.c
        pe/pe.c
        pe/rich.c
        module.c
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(${PROJECT_NAME} 
    PRIVATE
        redasm::redasm
)
