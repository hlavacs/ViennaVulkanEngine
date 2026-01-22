set(TARGET gaussian-splatting-cubemap-lighting-demo)
set(SOURCE gaussian-splatting-cubemap-lighting-demo.cpp)
set(HEADERS )

add_executable(${TARGET} ${SOURCE} ${HEADERS})
target_compile_features(${TARGET} PUBLIC cxx_std_20)

if(MSVC)
    set_property(TARGET ${TARGET} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endif()

target_link_libraries (${TARGET} PUBLIC viennavulkanengine)