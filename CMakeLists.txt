cmake_minimum_required(VERSION 3.21)

project(ViennaVulkanEngine VERSION 3.0.0 LANGUAGES CXX)

set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Vulkan REQUIRED)

file(GLOB_RECURSE VVE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cxx"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc"
)

if(NOT VVE_SOURCES)
    set(VVE_STUB_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/vve_stub.cpp")
    file(WRITE "${VVE_STUB_SOURCE}" "extern \"C\" void vve_stub() {}\n")
    list(APPEND VVE_SOURCES "${VVE_STUB_SOURCE}")
endif()

add_library(ViennaVulkanEngine SHARED ${VVE_SOURCES})
add_library(ViennaVulkanEngine::ViennaVulkanEngine ALIAS ViennaVulkanEngine)

target_include_directories(ViennaVulkanEngine
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(ViennaVulkanEngine
    PUBLIC
        Vulkan::Vulkan
)

if(MSVC)
    target_compile_options(ViennaVulkanEngine PRIVATE /std:c++latest)
else()
    target_compile_options(ViennaVulkanEngine PRIVATE -std=c++2c)
endif()

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

vve_set_output_dirs(ViennaVulkanEngine)
