cmake_minimum_required(VERSION 3.20)
project(redasm-loaders C)

if(NOT TARGET redasm::redasm)
    find_package(redasm REQUIRED)
endif()

if(WIN32)
    set(REDASM_INSTALL_PLUGINS "plugins/loaders")
else()
    include(GNUInstallDirs)
    set(REDASM_INSTALL_PLUGINS "${CMAKE_INSTALL_LIBDIR}/redasm/plugins/loaders")
endif()

add_subdirectory(android)
add_subdirectory(elf)
add_subdirectory(gameboy)
add_subdirectory(mz)
add_subdirectory(nes)
add_subdirectory(psx)
add_subdirectory(xbe)
add_subdirectory(zxspectrum)

get_property(subdirs DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" PROPERTY SUBDIRECTORIES)

foreach(subdir ${subdirs})
    get_property(targets DIRECTORY ${subdir} PROPERTY BUILDSYSTEM_TARGETS)

    foreach(target ${targets})
        get_target_property(target_type ${target} TYPE)

        if(target_type STREQUAL "SHARED_LIBRARY")
            if(NOT WIN32)
                set_target_properties(${target} PROPERTIES
                                      INSTALL_RPATH "$ORIGIN/../../../"
                )
            endif()

            install(TARGETS ${target}
                    LIBRARY DESTINATION "${REDASM_INSTALL_PLUGINS}"
                    RUNTIME DESTINATION "${REDASM_INSTALL_PLUGINS}"
            )
        endif()
    endforeach()
endforeach()
