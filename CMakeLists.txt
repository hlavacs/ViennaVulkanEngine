cmake_minimum_required(VERSION 3.28)

project(ViennaVulkanEngine VERSION 3.0.0 LANGUAGES CXX)

set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Vulkan REQUIRED)

function(vve_set_output_dirs target_name)
    get_target_property(target_type "${target_name}" TYPE)

    set_target_properties("${target_name}" PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/lib"
    )

    if(target_type STREQUAL "EXECUTABLE")
        set_target_properties("${target_name}" PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/exe"
        )
    elseif(target_type STREQUAL "SHARED_LIBRARY")
        set_target_properties("${target_name}" PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/dll"
            LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/dll"
        )
    elseif(target_type STREQUAL "MODULE_LIBRARY")
        set_target_properties("${target_name}" PROPERTIES
            LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/dll"
        )
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
