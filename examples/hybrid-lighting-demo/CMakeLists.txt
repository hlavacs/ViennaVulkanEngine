set(TARGET hybrid-lighting-demo)
set(SOURCE hybrid-lighting-demo.cpp)
set(HEADERS )

add_executable(${TARGET} ${SOURCE} ${HEADERS})
target_compile_features(${TARGET} PUBLIC cxx_std_20)

if(MSVC)
    set_property(TARGET ${TARGET} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endif()

target_link_libraries (${TARGET} PUBLIC viennavulkanengine)

# Auto-copy SDL3 DLL to executable directory (Debug: SDL3d.dll, Release: SDL3.dll)
if(WIN32)
    add_custom_command(TARGET ${TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:SDL3::SDL3>"
            "$<TARGET_FILE_DIR:${TARGET}>"
        COMMENT "Copying SDL3 DLL to ${TARGET} directory"
    )
endif()
