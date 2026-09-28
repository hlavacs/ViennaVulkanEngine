# Share example-only helpers without adding them to the engine facade.
add_library(vve_example_support STATIC)
target_compile_features(vve_example_support PUBLIC cxx_std_23)
target_sources(vve_example_support PUBLIC FILE_SET CXX_MODULES FILES common/ExampleSupport.ixx)
target_compile_definitions(vve_example_support PRIVATE VVE_ASSET_ROOT="${PROJECT_SOURCE_DIR}")
if(TARGET vve_windows_std)
    target_link_libraries(vve_example_support PUBLIC vve_windows_std)
endif()

# Register short facade-example runs with the same asset root as an interactive launch.
function(vve_add_example_test target)
    if(BUILD_TESTING AND VVE_ENGINE_IMPLEMENTATION_NAMESPACE STREQUAL "simple")
        if(TARGET ${target})
            set(capture_args)
            if(target STREQUAL "simple_forward_demo")
                set(capture_args --output "${CMAKE_CURRENT_BINARY_DIR}/${target}/verify/simple_forward_demo_capture.png")
            endif()
            add_test(NAME ${target} COMMAND ${target} --frames 3 ${capture_args})
            set_tests_properties(${target} PROPERTIES
                WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
                LABELS "gpu;example"
                TIMEOUT 60)
        endif()
    endif()
endfunction()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/game/CMakeLists.txt")
    add_subdirectory(game)
    vve_add_example_test(game)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/testscene/CMakeLists.txt")
    add_subdirectory(testscene)
    vve_add_example_test(testscene)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/sponza/CMakeLists.txt")
    add_subdirectory(sponza)
    vve_add_example_test(sponza)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/light_shadow_debug/CMakeLists.txt")
    add_subdirectory(light_shadow_debug)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/simple_forward_demo/CMakeLists.txt")
    add_subdirectory(simple_forward_demo)
    vve_add_example_test(simple_forward_demo)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/postprocessing/CMakeLists.txt")
    add_subdirectory(postprocessing)
endif()
