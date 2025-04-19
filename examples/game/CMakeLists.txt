set(TARGET game)
set(SOURCE game.cpp)
set(HEADERS )

add_executable(${TARGET} ${SOURCE} ${HEADERS})

target_compile_features(${TARGET} PUBLIC cxx_std_20)

# set the repository root directory as the debug working directory for this target in VS
if(MSVC)
    set_property(TARGET ${TARGET} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endif()

target_link_libraries (${TARGET} PUBLIC viennavulkanengine)

