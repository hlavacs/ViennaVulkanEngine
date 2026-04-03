cmake_minimum_required(VERSION 3.28)

if(NOT DEFINED VVE_VCPKG_TRIPLET OR VVE_VCPKG_TRIPLET STREQUAL "")
    if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
        set(VVE_VCPKG_TRIPLET_DEFAULT "x64-windows")
    elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
        set(VVE_VCPKG_TRIPLET_DEFAULT "x64-linux")
    elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
        set(VVE_VCPKG_TRIPLET_DEFAULT "x64-osx")
    else()
        message(FATAL_ERROR "Unsupported host platform '${CMAKE_HOST_SYSTEM_NAME}' for default vcpkg triplet selection.")
    endif()

    set(VVE_VCPKG_TRIPLET
        "${VVE_VCPKG_TRIPLET_DEFAULT}"
        CACHE STRING "Vcpkg triplet used for local manifest installs")
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/vcpkg_installed/${VVE_VCPKG_TRIPLET}")
    list(PREPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/vcpkg_installed/${VVE_VCPKG_TRIPLET}")
endif()

if(CMAKE_GENERATOR MATCHES "^Visual Studio")
    if(DEFINED CMAKE_VS_GLOBALS AND NOT CMAKE_VS_GLOBALS STREQUAL "")
        set(CMAKE_VS_GLOBALS "${CMAKE_VS_GLOBALS};VcpkgApplocalDeps=false")
    else()
        set(CMAKE_VS_GLOBALS "VcpkgApplocalDeps=false")
    endif()
endif()

project(ViennaVulkanEngine VERSION 3.0.0 LANGUAGES CXX)

set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(
    "$<$<CXX_COMPILER_ID:MSVC>:/MP>"
)

find_package(Doxygen QUIET)
if(DOXYGEN_FOUND)
    message(STATUS "Doxygen found: ${DOXYGEN_EXECUTABLE}")
else()
    message(STATUS "Doxygen not found. The 'docs' target will be unavailable.")
endif()

if(DEFINED ENV{VULKAN_SDK} AND NOT "$ENV{VULKAN_SDK}" STREQUAL "")
    list(PREPEND CMAKE_PREFIX_PATH "$ENV{VULKAN_SDK}/cmake")
endif()

find_package(Vulkan REQUIRED)
find_package(SDL3 CONFIG REQUIRED)

function(vve_set_output_dirs target_name)
    get_target_property(target_type "${target_name}" TYPE)

    set_target_properties("${target_name}" PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/lib"
    )
    foreach(config ${CMAKE_CONFIGURATION_TYPES})
        string(TOUPPER "${config}" config_upper)
        set_target_properties("${target_name}" PROPERTIES
            "ARCHIVE_OUTPUT_DIRECTORY_${config_upper}" "${CMAKE_SOURCE_DIR}/bin/lib"
        )
    endforeach()

    if(target_type STREQUAL "EXECUTABLE")
        set_target_properties("${target_name}" PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/exe"
        )
        foreach(config ${CMAKE_CONFIGURATION_TYPES})
            string(TOUPPER "${config}" config_upper)
            set_target_properties("${target_name}" PROPERTIES
                "RUNTIME_OUTPUT_DIRECTORY_${config_upper}" "${CMAKE_SOURCE_DIR}/bin/exe"
            )
        endforeach()

        if(WIN32)
            add_custom_command(TARGET "${target_name}" POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    $<TARGET_RUNTIME_DLLS:${target_name}>
                    $<TARGET_FILE_DIR:${target_name}>
                COMMAND_EXPAND_LISTS
            )
        endif()
    elseif(target_type STREQUAL "SHARED_LIBRARY")
        if(WIN32)
            set_target_properties("${target_name}" PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/exe"
            )
            foreach(config ${CMAKE_CONFIGURATION_TYPES})
                string(TOUPPER "${config}" config_upper)
                set_target_properties("${target_name}" PROPERTIES
                    "RUNTIME_OUTPUT_DIRECTORY_${config_upper}" "${CMAKE_SOURCE_DIR}/bin/exe"
                )
            endforeach()
        else()
            set_target_properties("${target_name}" PROPERTIES
                LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/lib"
            )
            foreach(config ${CMAKE_CONFIGURATION_TYPES})
                string(TOUPPER "${config}" config_upper)
                set_target_properties("${target_name}" PROPERTIES
                    "LIBRARY_OUTPUT_DIRECTORY_${config_upper}" "${CMAKE_SOURCE_DIR}/bin/lib"
                )
            endforeach()
        endif()
    elseif(target_type STREQUAL "MODULE_LIBRARY")
        set_target_properties("${target_name}" PROPERTIES
            LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/dll"
        )
        foreach(config ${CMAKE_CONFIGURATION_TYPES})
            string(TOUPPER "${config}" config_upper)
            set_target_properties("${target_name}" PROPERTIES
                "LIBRARY_OUTPUT_DIRECTORY_${config_upper}" "${CMAKE_SOURCE_DIR}/bin/dll"
            )
        endforeach()
    endif()
endfunction()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/CMakeLists.txt")
    add_subdirectory(src)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples/CMakeLists.txt")
    add_subdirectory(examples)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
    add_subdirectory(tests)
endif()

if(DOXYGEN_FOUND AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in")
    set(VVE_DOXYGEN_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/docs/build")
    set(VVE_DOXYGEN_INPUT
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/README.md"
    )
    string(REPLACE ";" " " VVE_DOXYGEN_INPUT "${VVE_DOXYGEN_INPUT}")

    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in"
        "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
        @ONLY
    )

    add_custom_target(docs
        COMMAND "${DOXYGEN_EXECUTABLE}" "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM
    )

    message(STATUS "Added CMake target: docs")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in")
    message(STATUS "Doxygen template found, but 'docs' target was not added because Doxygen is unavailable.")
endif()
