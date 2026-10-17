# Populate the ignored starter-kit folders before the game can be run.
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/RacerAssets.cmake")

# Physics and race rules without rendering, shared by the game and its CPU test.
add_library(racer_logic STATIC)
target_compile_features(racer_logic PUBLIC cxx_std_23)
target_sources(racer_logic PUBLIC FILE_SET CXX_MODULES FILES RacerLogic.ixx RacerRules.ixx)
if(TARGET vve_windows_std)
    target_link_libraries(racer_logic PUBLIC vve_windows_std)
endif()

add_executable(racer_logic_tests racer_logic_tests.cpp)
target_link_libraries(racer_logic_tests PRIVATE racer_logic)
add_dependencies(racer_logic_tests racer_logic)
vve_set_output_dirs(racer_logic_tests)

if(BUILD_TESTING)
    add_test(NAME RacerLogicTests COMMAND racer_logic_tests)
    set_tests_properties(RacerLogicTests PROPERTIES LABELS example TIMEOUT 60)
endif()

# The game: two user systems over the facade, the logic module and the scene module.
add_executable(racer racer.cpp)
target_sources(racer PRIVATE FILE_SET CXX_MODULES FILES RacerScene.ixx RacerWorld.ixx RacerRace.ixx RacerSound.ixx RacerView.ixx)
target_link_libraries(racer PRIVATE ViennaVulkanEngine::ViennaVulkanEngine vve_example_support racer_logic)
add_dependencies(racer ViennaVulkanEngine vve_simple_shaders)
vve_set_output_dirs(racer)

# The four game runs take a while, so they carry the label "racer" and the build scripts skip them (ctest -LE racer).
# Run them with: ctest --test-dir <build directory> -L racer --output-on-failure
if(BUILD_TESTING AND VVE_ENGINE_IMPLEMENTATION_NAMESPACE STREQUAL "simple")
    # Bounded autopilot run: 15 s of racing, a capture and a verification file; fails on a wall breach or a bad pick.
    add_test(NAME RacerExample COMMAND racer --frames 900 --autopilot
        --output "${CMAKE_CURRENT_BINARY_DIR}/verify/racer.png" --verify "${CMAKE_CURRENT_BINARY_DIR}/verify/racer.txt")
    add_test(NAME RacerNightExample COMMAND racer --frames 600 --autopilot --night
        --output "${CMAKE_CURRENT_BINARY_DIR}/verify/racer_night.png" --verify "${CMAKE_CURRENT_BINARY_DIR}/verify/racer_night.txt")
    add_test(NAME RacerMenuExample COMMAND racer --frames 3 --menu
        --output "${CMAKE_CURRENT_BINARY_DIR}/verify/racer_menu.png" --verify "${CMAKE_CURRENT_BINARY_DIR}/verify/racer_menu.txt")
    # A solo lap of about 30 s makes the first ghost; the run ends while the ghost drives.
    add_test(NAME RacerGhostExample COMMAND racer --frames 2400 --autopilot --time-trial
        --output "${CMAKE_CURRENT_BINARY_DIR}/verify/racer_ghost.png" --verify "${CMAKE_CURRENT_BINARY_DIR}/verify/racer_ghost.txt")
    set_tests_properties(RacerExample RacerNightExample RacerMenuExample RacerGhostExample PROPERTIES
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" LABELS "gpu;example;racer" ENVIRONMENT "SDL_AUDIO_DRIVER=dummy" TIMEOUT 120)
endif()
