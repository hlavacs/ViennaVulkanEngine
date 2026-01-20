set(TARGET gaussian-splatting-cubemap-lighting-demo)
set(SOURCE gaussian-splatting-cubemap-lighting-demo.cpp)
set(HEADERS )

add_executable(${TARGET} ${SOURCE} ${HEADERS})
target_compile_features(${TARGET} PUBLIC cxx_std_20)

if(MSVC)
    set_property(TARGET ${TARGET} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endif()

target_link_libraries (${TARGET} PUBLIC viennavulkanengine)

# SDL3 Issue #14867: SDL_STATIC/SDL_SHARED don't work reliably, DLL still produced.
# Copy SDL3 DLL to executable directory as workaround.
if(WIN32)
    add_custom_command(TARGET ${TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:SDL3::SDL3>"
            "$<TARGET_FILE_DIR:${TARGET}>"
        COMMENT "Copying SDL3 DLL to ${TARGET} directory"
    )
endif()
