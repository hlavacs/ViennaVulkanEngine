# Island Air's external assets live in a build cache, never in the source tree.
# Every download is checked before use; an intact cache works without a network.
set(VVE_ISLAND_ASSET_DIR "${CMAKE_BINARY_DIR}/_assets/island_delivery" CACHE PATH
    "Downloaded Island Air models, photographs and recordings")
option(VVE_ISLAND_DOWNLOAD_ASSETS "Download missing Island Air assets while configuring" ON)
get_filename_component(VVE_ISLAND_ASSET_DIR "${VVE_ISLAND_ASSET_DIR}" ABSOLUTE BASE_DIR "${CMAKE_BINARY_DIR}")

# Publish downloads atomically so a failed transfer cannot become a valid cache hit.
function(vve_island_asset name url sha256)
    set(destination "${VVE_ISLAND_ASSET_DIR}/${name}")
    if(EXISTS "${destination}")
        file(SHA256 "${destination}" actual)
        if(actual STREQUAL sha256)
            return()
        endif()
    endif()
    if(NOT VVE_ISLAND_DOWNLOAD_ASSETS)
        message(FATAL_ERROR "Island Air asset missing or changed: ${destination}. Enable VVE_ISLAND_DOWNLOAD_ASSETS to restore it.")
    endif()
    get_filename_component(directory "${destination}" DIRECTORY)
    file(MAKE_DIRECTORY "${directory}")
    message(STATUS "Downloading Island Air asset: ${name}")
    file(DOWNLOAD "${url}" "${destination}.download"
        STATUS status TLS_VERIFY ON TIMEOUT 120 INACTIVITY_TIMEOUT 30)
    list(GET status 0 error)
    if(NOT error EQUAL 0)
        file(REMOVE "${destination}.download")
        message(FATAL_ERROR "Island Air could not download ${url}: ${status}")
    endif()
    file(SHA256 "${destination}.download" actual)
    if(NOT actual STREQUAL sha256)
        file(REMOVE "${destination}.download")
        message(FATAL_ERROR "Island Air SHA256 mismatch for ${name}; expected ${sha256}, received ${actual}")
    endif()
    file(RENAME "${destination}.download" "${destination}")
endfunction()

# Photographs and surface scans: Poly Haven, CC0 (https://polyhaven.com/license).
# The aircraft and upstream license are pinned to the same Cesium commit.
vve_island_asset(sky.jpg
    "https://dl.polyhaven.org/file/ph-assets/HDRIs/extra/Tonemapped%20JPG/kloofendal_48d_partly_cloudy_puresky.jpg"
    2bdd7abe32f1d2f7a626e0a6e45bb808313f29bd41d758eacbdba91560e93f04)

vve_island_asset(grass.jpg
    "https://dl.polyhaven.org/file/ph-assets/Textures/jpg/1k/leafy_grass/leafy_grass_diff_1k.jpg"
    cfa40bc9d9417d1852db8753a8d5917f110c40101179f63543c382e39bc05e4a)

vve_island_asset(sand.jpg
    "https://dl.polyhaven.org/file/ph-assets/Textures/jpg/1k/coast_sand_01/coast_sand_01_diff_1k.jpg"
    58af2e40100f7f405703a86d593af64379f822c4786850def23ecfc921450d54)

vve_island_asset(rock.jpg
    "https://dl.polyhaven.org/file/ph-assets/Textures/jpg/1k/rock_face/rock_face_diff_1k.jpg"
    cce4b50517161264bdef196f5e247e328ca3739083cd6044ad3cc54d88cb82e2)

vve_island_asset(asphalt.jpg
    "https://dl.polyhaven.org/file/ph-assets/Textures/jpg/1k/asphalt_track/asphalt_track_diff_1k.jpg"
    05c4e79cd99160075969d37bfc6ef72be262153a410bb45510b2c23f7303894c)

vve_island_asset(concrete.jpg
    "https://dl.polyhaven.org/file/ph-assets/Textures/jpg/1k/concrete_floor_02/concrete_floor_02_diff_1k.jpg"
    6db84a6d87dc50508b9d134d6fbd2b7e876b8081a5f43a118a249ca18de73af5)

vve_island_asset(planks.jpg
    "https://dl.polyhaven.org/file/ph-assets/Textures/jpg/1k/weathered_planks/weathered_planks_diff_1k.jpg"
    01c9a372cfd7a7bf471b59c2458ed3ccecc93448aebf7ba0cc5b3b5b6ed24349)

vve_island_asset(metal.jpg
    "https://dl.polyhaven.org/file/ph-assets/Textures/jpg/1k/corrugated_iron/corrugated_iron_diff_1k.jpg"
    7f3649dd0d056a55a910146f09b1d5298370696a90f511a255fb4c5715bb4b58)

vve_island_asset(aircraft.glb
    "https://raw.githubusercontent.com/CesiumGS/cesium/85500087b890479ffa3bf0b4cf0db55887c2d89a/Apps/SampleData/models/CesiumAir/Cesium_Air.glb"
    e72f627c5f0c9dc50726059703df55d74e634a32dbccb25b18f1783f913360b8)

vve_island_asset(Cesium-LICENSE.md
    "https://raw.githubusercontent.com/CesiumGS/cesium/85500087b890479ffa3bf0b4cf0db55887c2d89a/LICENSE.md"
    721844bc0cdaa9b1a9a145dbca1f981e2de15a7e200a5d1fd9f5f1eed8e5d030)

# Kenney's CC0 service vehicles: pin both the models and their relative palette texture.
set(island_kenney_racing "https://raw.githubusercontent.com/KenneyNL/Starter-Kit-Racing/2f2e5f2646dda89cb21d4e8539bab60c6e955dc8/models")
vve_island_asset(vehicles/vehicle-truck-yellow.glb
    "${island_kenney_racing}/vehicle-truck-yellow.glb"
    1ebd83174eab6d2fdf69eb5d8e32bd06a23a86d9edc4fe915a4160430a7c36a5)
vve_island_asset(vehicles/vehicle-truck-green.glb
    "${island_kenney_racing}/vehicle-truck-green.glb"
    df362d027a09b19395be0e96abd7cc2dc54608dfb920b592e64357b589f9fcd6)
vve_island_asset(vehicles/Textures/colormap.png
    "${island_kenney_racing}/Textures/colormap.png"
    02bb3fb87365927d16e265d27caae405aeef5819b6bde5a989ff5e435318de4d)

# Reuse the repository's existing rigid Kenney Mini Characters, already prepared for node animation.
# This adds no binary models to Git and needs neither Python nor a model conversion step to build.
foreach(character character-male-a.glb character-male-b.glb character-female-a.glb character-female-b.glb colormap.png)
    configure_file("${PROJECT_SOURCE_DIR}/assets/racer/crowd/${character}"
        "${VVE_ISLAND_ASSET_DIR}/people/${character}" COPYONLY)
endforeach()

# Recorded aircraft and location sound; authors and selected licenses travel with the executable.
vve_island_asset(sounds/propeller.ogg
    "https://opengameart.org/sites/default/files/airplane_prop_0.ogg"
    f835c4f5ab4233058af5bd98fa58da2415ac174a26c29cdeca791bd4e8fa45f8)
vve_island_asset(sounds/cockpit.mp3
    "https://cdn.freesound.org/previews/188/188423_213393-hq.mp3"
    5f7a12541fcd3882ce6fb83969e278ec6937e44479ef192923d67093ef5723fc)
vve_island_asset(sounds/wind.mp3
    "https://opengameart.org/sites/default/files/strong%20wind%20blowing_0.mp3"
    261f8a92a9e9c76f8e2f7fa343f583a466511def26f2ff366048931c37f5b542)
vve_island_asset(sounds/coast.mp3
    "https://cdn.freesound.org/previews/18/18363_71309-hq.mp3"
    22af26bb4e66915308bcc2a3c1b329161ef1bbd443c5f51cd85ee02de6ee2636)
vve_island_asset(sounds/rolling.mp3
    "https://cdn.freesound.org/previews/653/653110_7913959-hq.mp3"
    716f029188f98b974a10ca259cc6c8666223db50ff12e8cebf4438e160bce10b)
vve_island_asset(sounds/touchdown.wav
    "https://opengameart.org/sites/default/files/jumpland.wav"
    b4d262eb5c048167cceca9f5d790f6ad9310c14588ce87f5466c0a978228a4ca)
vve_island_asset(sounds/crash.ogg
    "https://opengameart.org/sites/default/files/qubodup-crash.ogg"
    e468ee00a6c94313cf4efcf347b5f4e51e98e629608be277a8f58feb5e2267ff)
vve_island_asset(sounds/unload.mp3
    "https://cdn.freesound.org/previews/690/690010_13350855-hq.mp3"
    463e25609885148e38e6c2d5bec1aeb960c5894842be25c3d62e90f5bfed532a)

