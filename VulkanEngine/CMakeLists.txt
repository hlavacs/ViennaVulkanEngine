
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

file(GLOB VulkanEngineSRC ./*.cpp ./*.c ./*.h)
add_library(vulkanengine STATIC ${VulkanEngineSRC} ${CMAKE_SOURCE_DIR}/external/threadpool/ThreadPool.cpp)

#include directories
target_include_directories(vulkanengine PUBLIC ${CMAKE_SOURCE_DIR}/external/Assimp/include)
target_include_directories(vulkanengine PUBLIC ${CMAKE_SOURCE_DIR}/external/glfw/include)
target_include_directories(vulkanengine PUBLIC ${CMAKE_SOURCE_DIR}/external/glm)
target_include_directories(vulkanengine PUBLIC ${CMAKE_SOURCE_DIR}/external/nuklear)
target_include_directories(vulkanengine PUBLIC ${CMAKE_SOURCE_DIR}/external/stb)
target_include_directories(vulkanengine PUBLIC ${CMAKE_SOURCE_DIR}/external/threadpool)

find_package(Vulkan REQUIRED)
include_directories(${Vulkan_INCLUDE_DIRS})
target_link_libraries(vulkanengine ${Vulkan_LIBRARY})

#Link Assimp link libraries
set( AssimpDir ${CMAKE_SOURCE_DIR}/external/Assimp/lib/x64)
set( AssimpLibRel ${AssimpDir}/Release/assimp-vc140-mt.lib ${AssimpDir}/Release/IrrXML.lib ${AssimpDir}/Release/zlib.lib )
set( AssimpLibDeb ${AssimpDir}/Debug/assimp-vc140-mt.lib ${AssimpDir}/Debug/IrrXML.lib ${AssimpDir}/Debug/zlibd.lib )
add_library(AssimpLib STATIC IMPORTED)

set_target_properties(AssimpLib PROPERTIES
             IMPORTED_CONFIGURATIONS "Debug;Release"
             IMPORTED_LOCATION ${AssimpLibRel}
             IMPORTED_LOCATION_DEBUG ${AssimpLibDeb}
             )

set_target_properties(vulkanengine PROPERTIES
            LIBRARY_OUTPUT_DIRECTORY ../lib
            ARCHIVE_OUTPUT_DIRECTORY ../lib
            )

target_link_libraries(vulkanengine AssimpLib ${CMAKE_SOURCE_DIR}/external/glfw/lib-vc2022/glfw3.lib )
