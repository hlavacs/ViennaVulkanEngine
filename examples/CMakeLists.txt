if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/game/CMakeLists.txt")
    add_subdirectory(game)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/physics/CMakeLists.txt")
    add_subdirectory(physics)
endif()
