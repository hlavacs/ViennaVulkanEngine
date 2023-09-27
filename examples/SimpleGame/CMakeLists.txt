

include_directories(${CMAKE_SOURCE_DIR}/VulkanEngine ${CMAKE_SOURCE_DIR}/external/irrKlang/include)

add_executable(simplegame simplegame.cpp)

target_include_directories(simplegame PUBLIC ${CMAKE_SOURCE_DIR}/external/Assimp/include)
target_include_directories(simplegame PUBLIC ${CMAKE_SOURCE_DIR}/external/glfw/include)
target_include_directories(simplegame PUBLIC ${CMAKE_SOURCE_DIR}/external/glm)
target_include_directories(simplegame PUBLIC ${CMAKE_SOURCE_DIR}/external/nuklear)
target_include_directories(simplegame PUBLIC ${CMAKE_SOURCE_DIR}/external/stb)
target_include_directories(simplegame PUBLIC ${CMAKE_SOURCE_DIR}/external/threadpool)

find_package(Vulkan REQUIRED)
include_directories(${Vulkan_INCLUDE_DIRS})

set_target_properties(simplegame PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
            )

add_library(vve STATIC IMPORTED)
set_target_properties(vve PROPERTIES
           IMPORTED_CONFIGURATIONS "Debug;Release"
           IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/lib/Release/vulkanengine.lib
           IMPORTED_LOCATION_DEBUG ${CMAKE_SOURCE_DIR}/lib/Debug/vulkanengine.lib
           )

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

target_link_libraries(simplegame
          vve
          AssimpLib
          ${CMAKE_SOURCE_DIR}/external/irrKlang/lib/irrKlang.lib
          ${CMAKE_SOURCE_DIR}/external/glfw/lib-vc2022/glfw3.lib
          )

find_package(Vulkan REQUIRED)


#set( CMAKE_VS_SDK_EXECUTABLE_DIRECTORIES $(CMAKE_VS_SDK_EXECUTABLE_DIRECTORIES);$(VULKAN_SDK);)

file(COPY ${CMAKE_SOURCE_DIR}/external/Assimp/lib/x64/Debug/assimp-vc140-mt.dll DESTINATION ${CMAKE_SOURCE_DIR}/bin/Debug)
file(COPY ${CMAKE_SOURCE_DIR}/external/Assimp/lib/x64/Release/assimp-vc140-mt.dll DESTINATION ${CMAKE_SOURCE_DIR}/bin/Release)

file(COPY ${CMAKE_SOURCE_DIR}/external/irrKlang/bin/ikpFlac.dll DESTINATION ${CMAKE_SOURCE_DIR}/bin/Debug)
file(COPY ${CMAKE_SOURCE_DIR}/external/irrKlang/bin/ikpMP3.dll DESTINATION ${CMAKE_SOURCE_DIR}/bin/Debug)
file(COPY ${CMAKE_SOURCE_DIR}/external/irrKlang/bin/irrKlang.dll DESTINATION ${CMAKE_SOURCE_DIR}/bin/Debug)
file(COPY ${CMAKE_SOURCE_DIR}/external/irrKlang/bin/ikpFlac.dll DESTINATION ${CMAKE_SOURCE_DIR}/bin/Release)
file(COPY ${CMAKE_SOURCE_DIR}/external/irrKlang/bin/ikpMP3.dll DESTINATION ${CMAKE_SOURCE_DIR}/bin/Release)
file(COPY ${CMAKE_SOURCE_DIR}/external/irrKlang/bin/irrKlang.dll DESTINATION ${CMAKE_SOURCE_DIR}/bin/Release)
