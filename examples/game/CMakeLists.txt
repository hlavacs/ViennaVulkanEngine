add_executable(game
    game.cpp
)

target_link_libraries(game PRIVATE
    ViennaVulkanEngine::ViennaVulkanEngine
    vve_example_support
)

add_dependencies(game
    ViennaVulkanEngine
    vve_simple_shaders
)

vve_set_output_dirs(game)

# Keep V2's soundtrack and attribution beside the executable. These are build
# operations; the game opens the files itself without a launch script.
set(game_music_files)
foreach(name dance.mp3 ophelia.mp3 getout.ogg license.txt V2_MUSIC.md)
    list(APPEND game_music_files "${PROJECT_SOURCE_DIR}/assets/sounds/${name}")
endforeach()
add_custom_target(game_audio_assets ALL
    COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:game>/audio/v2"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        ${game_music_files} "$<TARGET_FILE_DIR:game>/audio/v2"
    VERBATIM)
add_dependencies(game game_audio_assets)

if(BUILD_TESTING)
    foreach(track RANGE 0 2)
        add_test(NAME GameMusic${track} COMMAND game --frames 3 --music-track ${track} --require-audio)
        set_tests_properties(GameMusic${track} PROPERTIES
            ENVIRONMENT "SDL_AUDIO_DRIVER=dummy" LABELS "gpu;audio;example" TIMEOUT 60
            WORKING_DIRECTORY "$<TARGET_FILE_DIR:game>")
    endforeach()
endif()
