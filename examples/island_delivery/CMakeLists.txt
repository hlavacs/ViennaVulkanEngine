# Configure-time downloads stay under the build tree and are reused after SHA256 validation.
include("${PROJECT_SOURCE_DIR}/cmake/IslandDeliveryAssets.cmake")
configure_file(AUDIO_LICENSE.txt "${VVE_ISLAND_ASSET_DIR}/AUDIO_LICENSE.txt" COPYONLY)
configure_file(ASSET_LICENSES.txt "${VVE_ISLAND_ASSET_DIR}/ASSET_LICENSES.txt" COPYONLY)

# Share the deterministic physics and delivery rules with headless C++ tests.
add_library(island_delivery_logic STATIC)
target_compile_features(island_delivery_logic PUBLIC cxx_std_23)
target_sources(island_delivery_logic PUBLIC FILE_SET CXX_MODULES FILES IslandLogic.ixx)
target_link_libraries(island_delivery_logic PUBLIC ViennaVulkanEngine::ViennaVulkanEngine)

# The game imports the facade; Vulkan and SDL remain the engine's responsibility.
add_library(island_delivery_game STATIC)
target_compile_features(island_delivery_game PUBLIC cxx_std_23)
target_sources(island_delivery_game PUBLIC FILE_SET CXX_MODULES FILES IslandScene.ixx IslandAirport.ixx IslandSound.ixx IslandMap.ixx IslandGame.ixx)
target_link_libraries(island_delivery_game PUBLIC island_delivery_logic)
add_executable(island_delivery island_delivery.cpp)
target_link_libraries(island_delivery PRIVATE island_delivery_game vve_example_support)
target_compile_definitions(island_delivery PRIVATE VVE_ISLAND_ASSET_DIR="${VVE_ISLAND_ASSET_DIR}")
add_dependencies(island_delivery ViennaVulkanEngine vve_simple_shaders)
vve_set_output_dirs(island_delivery)

# Keep a built executable usable from any working directory, with licenses beside its downloaded assets.
add_custom_target(island_delivery_assets
    COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different
        "${VVE_ISLAND_ASSET_DIR}" "$<TARGET_FILE_DIR:island_delivery>/assets/island_delivery"
    VERBATIM)
add_dependencies(island_delivery island_delivery_assets)

if(BUILD_TESTING)
    add_test(NAME IslandDeliveryHelp COMMAND island_delivery --help)
    add_test(NAME IslandDeliveryInvalidOptions COMMAND island_delivery --frames invalid)
    set_tests_properties(IslandDeliveryInvalidOptions PROPERTIES WILL_FAIL TRUE)
    # Advance the full mission through actual simulation steps; only presentation is sampled sparsely.
    vve_add_renderer_example_test(IslandDeliverySmoke island_delivery --frames 3 --hidden --width 640 --height 360
        --output "${CMAKE_CURRENT_BINARY_DIR}/verify/start.png" --verify "${CMAKE_CURRENT_BINARY_DIR}/verify/start.csv")
    vve_add_renderer_example_test(IslandDeliveryAutopilot island_delivery --frames 60 --steps-per-frame 240 --autopilot
        --require-delivery --hidden --width 640 --height 360
        --output "${CMAKE_CURRENT_BINARY_DIR}/verify/delivered.png" --verify "${CMAKE_CURRENT_BINARY_DIR}/verify/delivered.csv")
    vve_add_renderer_example_test(IslandDeliveryCockpit island_delivery --frames 3 --camera cockpit --hidden --width 640 --height 360
        --output "${CMAKE_CURRENT_BINARY_DIR}/verify/cockpit.png" --verify "${CMAKE_CURRENT_BINARY_DIR}/verify/cockpit.csv")
    vve_add_renderer_example_test(IslandDeliveryMap island_delivery --frames 3 --map --destination 255 --hidden --width 960 --height 640
        --output "${CMAKE_CURRENT_BINARY_DIR}/verify/map.png" --verify "${CMAKE_CURRENT_BINARY_DIR}/verify/map.csv")
    vve_add_renderer_example_test(IslandDeliveryAirport island_delivery --frames 3 --steps-per-frame 120 --camera airport --hidden --width 1280 --height 720
        --output "${CMAKE_CURRENT_BINARY_DIR}/verify/airport.png" --verify "${CMAKE_CURRENT_BINARY_DIR}/verify/airport.csv")
    vve_add_renderer_example_test(IslandDeliveryCargoLoop island_delivery --frames 60 --steps-per-frame 1200 --autopilot
        --deliveries 2 --require-delivery --hidden --width 640 --height 360
        --output "${CMAKE_CURRENT_BINARY_DIR}/verify/cargo_loop.png" --verify "${CMAKE_CURRENT_BINARY_DIR}/verify/cargo_loop.csv")
    vve_add_renderer_example_test(IslandDeliveryRemoteAirport island_delivery --frames 60 --steps-per-frame 1200 --autopilot
        --start-island 254 --require-delivery --hidden --width 640 --height 360
        --output "${CMAKE_CURRENT_BINARY_DIR}/verify/remote.png" --verify "${CMAKE_CURRENT_BINARY_DIR}/verify/remote.csv")
    set_tests_properties(IslandDeliverySmoke IslandDeliveryAutopilot IslandDeliveryCockpit
        IslandDeliveryMap IslandDeliveryAirport IslandDeliveryCargoLoop IslandDeliveryRemoteAirport PROPERTIES
        LABELS "gpu;example;island_delivery" ENVIRONMENT "SDL_AUDIO_DRIVER=dummy" TIMEOUT 180)
    set_tests_properties(IslandDeliveryAirport PROPERTIES PASS_REGULAR_EXPRESSION "airport_people=12 airport_vehicles=3")
    set_tests_properties(IslandDeliverySmoke PROPERTIES
        PASS_REGULAR_EXPRESSION "passed=true delivered=false deliveries=0 origin=0 target=1.*map_open=false takeoffs=0 landings=0")
    set_tests_properties(IslandDeliveryMap PROPERTIES PASS_REGULAR_EXPRESSION "passed=true.*map_open=true")
endif()
