# The game needs only VVE. SDL3_mixer is linked by the engine implementation.
add_executable(audio_demo audio.cpp)
target_link_libraries(audio_demo PRIVATE ViennaVulkanEngine::ViennaVulkanEngine)
vve_set_output_dirs(audio_demo)

# Keep the default sound next to the executable; launching needs no script or working directory.
add_custom_command(TARGET audio_demo POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:audio_demo>/audio"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${PROJECT_SOURCE_DIR}/assets/sounds/demo.wav" "$<TARGET_FILE_DIR:audio_demo>/audio/demo.wav"
    VERBATIM)

set_property(TARGET audio_demo APPEND PROPERTY LINK_DEPENDS "${PROJECT_SOURCE_DIR}/assets/sounds/demo.wav")

if(BUILD_TESTING)
    add_test(NAME AudioDemo COMMAND audio_demo --verify)
    set_tests_properties(AudioDemo PROPERTIES
        ENVIRONMENT "SDL_AUDIO_DRIVER=dummy" LABELS "audio;example" TIMEOUT 15
        WORKING_DIRECTORY "$<TARGET_FILE_DIR:audio_demo>")
endif()
