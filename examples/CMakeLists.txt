if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/game/CMakeLists.txt")
    add_subdirectory(game)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/testscene/CMakeLists.txt")
    add_subdirectory(testscene)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/physics/CMakeLists.txt")
    add_subdirectory(physics)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/sponza/CMakeLists.txt")
    add_subdirectory(sponza)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/light_shadow_debug/CMakeLists.txt")
    add_subdirectory(light_shadow_debug)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/simple_forward_demo/CMakeLists.txt")
    add_subdirectory(simple_forward_demo)
endif()
