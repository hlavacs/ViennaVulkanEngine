cmake_minimum_required(VERSION 3.28)

set(CMAKE_VS_GLOBALS "VcpkgEnabled=false")

project(ViennaVulkanEngine VERSION 3.0.0 LANGUAGES CXX)

set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(
    "$<$<CXX_COMPILER_ID:MSVC>:/MP>"
)

find_package(Vulkan REQUIRED)

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
    elseif(target_type STREQUAL "SHARED_LIBRARY")
        set_target_properties("${target_name}" PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/exe"
            LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/dll"
        )
        foreach(config ${CMAKE_CONFIGURATION_TYPES})
            string(TOUPPER "${config}" config_upper)
            set_target_properties("${target_name}" PROPERTIES
                "RUNTIME_OUTPUT_DIRECTORY_${config_upper}" "${CMAKE_SOURCE_DIR}/bin/exe"
                "LIBRARY_OUTPUT_DIRECTORY_${config_upper}" "${CMAKE_SOURCE_DIR}/bin/dll"
            )
        endforeach()
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
