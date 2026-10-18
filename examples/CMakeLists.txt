# Share example-only helpers without adding them to the engine facade.
add_library(vve_example_support STATIC)
target_compile_features(vve_example_support PUBLIC cxx_std_23)
target_sources(vve_example_support PUBLIC FILE_SET CXX_MODULES FILES common/ExampleSupport.ixx)
target_link_libraries(vve_example_support PUBLIC ViennaVulkanEngine::ViennaVulkanEngine)
target_compile_definitions(vve_example_support PRIVATE VVE_ASSET_ROOT="${PROJECT_SOURCE_DIR}")
if(TARGET vve_windows_std)
    target_link_libraries(vve_example_support PUBLIC vve_windows_std)
endif()

# Run the same executable with both public backends; keep original forward test names and capture paths.
function(vve_add_renderer_example_test name target)
    add_test(NAME ${name} COMMAND ${target} ${ARGN} --renderer forward)
    set(deferred_args)
    foreach(argument IN LISTS ARGN)
        string(REGEX REPLACE "\\.(png|txt|csv)$" "_deferred.\\1" argument "${argument}")
        list(APPEND deferred_args "${argument}")
    endforeach()
    add_test(NAME ${name}Deferred COMMAND ${target} ${deferred_args} --renderer deferred)
    set(labels "gpu;example")
    if(target STREQUAL "racer")
        list(APPEND labels racer)
    endif()
    set_tests_properties(${name}Deferred PROPERTIES
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" LABELS "${labels}"
        ENVIRONMENT "SDL_AUDIO_DRIVER=dummy" TIMEOUT 180)
    set_tests_properties(${name} PROPERTIES ENVIRONMENT "SDL_AUDIO_DRIVER=dummy")
endfunction()

add_subdirectory(audio)

# Register short facade-example runs with the same asset root as an interactive launch.
function(vve_add_example_test target)
    if(BUILD_TESTING AND VVE_ENGINE_IMPLEMENTATION_NAMESPACE STREQUAL "simple")
        if(TARGET ${target})
            set(capture_args --output "${CMAKE_CURRENT_BINARY_DIR}/${target}/verify/${target}.png")
            if(target STREQUAL "simple_forward_demo")
                set(capture_args --output "${CMAKE_CURRENT_BINARY_DIR}/${target}/verify/simple_forward_demo_capture.png")
            endif()
            set(frame_count 3)
            if(target STREQUAL "game")
                set(frame_count 180) # Exercise falling crates and creation after rendering has started.
                list(APPEND capture_args --verify-collection) # Collect the first crate through the game's ordinary removal path.
            endif()
            vve_add_renderer_example_test(${target} ${target} --frames ${frame_count} ${capture_args})
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
    if(BUILD_TESTING)
        add_test(NAME LightShadowDebugDeferred COMMAND light_shadow_debug --renderer deferred --frames 3
            --output "${CMAKE_CURRENT_BINARY_DIR}/light_shadow_debug/verify/deferred.txt")
        set_tests_properties(LightShadowDebugDeferred PROPERTIES LABELS "gpu;example" TIMEOUT 60)
    endif()
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/simple_forward_demo/CMakeLists.txt")
    add_subdirectory(simple_forward_demo)
    vve_add_example_test(simple_forward_demo)
endif()

# The car example checks every frame that its parts follow their parent nodes and fails on a mismatch.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/hierarchy_car/CMakeLists.txt")
    add_subdirectory(hierarchy_car)
    if(BUILD_TESTING AND VVE_ENGINE_IMPLEMENTATION_NAMESPACE STREQUAL "simple")
        vve_add_renderer_example_test(HierarchyCarExample hierarchy_car --frames 120
            --output "${CMAKE_CURRENT_BINARY_DIR}/hierarchy_car/verify/hierarchy_car.png"
            --verify "${CMAKE_CURRENT_BINARY_DIR}/hierarchy_car/verify/hierarchy_car.txt")
        set_tests_properties(HierarchyCarExample PROPERTIES
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
            LABELS "gpu;example"
            TIMEOUT 60)
    endif()
endif()

# Ring Racer: a lap-racing game; its logic has a CPU test and the game a bounded autopilot run.
add_subdirectory(racer)

# Island Air: ECS flight, conventional island airstrips and landing-only cargo deliveries.
add_subdirectory(island_delivery)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/postprocessing/CMakeLists.txt")
    add_subdirectory(postprocessing)
    if(BUILD_TESTING)
        vve_add_renderer_example_test(PostProcessingCapture postprocessing --frames 3 --all-effects
            --output "${CMAKE_CURRENT_BINARY_DIR}/postprocessing/verify/all_effects.png")
        set_tests_properties(PostProcessingCapture PROPERTIES LABELS "gpu;example" TIMEOUT 60)
    endif()
endif()
