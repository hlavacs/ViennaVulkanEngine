file(GLOB_RECURSE VVE_TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cxx"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cc"
)

if(VVE_TEST_SOURCES)
    enable_testing()

    foreach(test_source IN LISTS VVE_TEST_SOURCES)
        get_filename_component(test_name "${test_source}" NAME_WE)

        add_executable("${test_name}" "${test_source}")
        target_link_libraries("${test_name}" PRIVATE ViennaVulkanEngine::ViennaVulkanEngine)

        add_test(NAME "${test_name}" COMMAND "${test_name}")
        vve_set_output_dirs("${test_name}")
    endforeach()
endif()
