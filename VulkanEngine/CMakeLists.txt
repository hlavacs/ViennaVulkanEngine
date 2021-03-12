cmake_minimum_required(VERSION 3.7.1)
project(vienna_vulkan_engine_cmake)

set(CMAKE_CXX_STANDARD 20)

include_directories(
        ../external/stb
        ../external/threadpool

        #brew headers
        /usr/local/include
        #vulcan headers
        ~/Code/CLionProjects/vulkanSDK/macOS/include
)
link_directories(
        #brew lib
        /usr/local/lib
        #vulcan lib
        ~/Code/CLionProjects/vulkanSDK/macOS/lib
)

# add the executable
file(GLOB SOURCE_FILES CONFIGURE_DEPENDS "*.h" "*.cpp")
add_executable(
        app
        ${SOURCE_FILES}
        ../external/threadpool/ThreadPool.h
        ../external/threadpool/ThreadPool.cpp
)

target_link_libraries(app vulkan glfw assimp)
