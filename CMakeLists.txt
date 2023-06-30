
cmake_minimum_required (VERSION 3.0.0)
project (Minimal LANGUAGES CXX)

find_package(Vulkan REQUIRED)

if (MSVC)
  set(CMAKE_CXX_FLAGS_RELEASE /O2)
  set(CMAKE_VS_PLATFORM_NAME "x64")
  set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

include_directories ($ENV{VULKAN_SDK}/include)
link_directories($ENV{VULKAN_SDK}/lib)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_HOME_DIRECTORY}/bin)

set(TARGET Minimal)
set(SOURCE Minimal.cpp)
set(HEADERS)
add_executable(${TARGET} ${SOURCE} ${HEADERS})

target_compile_features(${TARGET} PUBLIC cxx_std_20)
target_link_libraries(${TARGET} PRIVATE SDL2.lib SDL2main.lib shaderc.lib vulkan-1.lib)
set_target_properties(
    ${TARGET}
    PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${EXECUTABLE_OUTPUT_PATH}"
)

#-----------------------------------------------------------------------------------------

set_property(DIRECTORY "${PROJECT_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT "Minimal")

