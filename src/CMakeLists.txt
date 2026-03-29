file(GLOB_RECURSE VVE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cxx"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cc"
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
        "${CMAKE_SOURCE_DIR}/include"
)

target_link_libraries(ViennaVulkanEngine
    PUBLIC
        Vulkan::Vulkan
)

vve_set_output_dirs(ViennaVulkanEngine)
