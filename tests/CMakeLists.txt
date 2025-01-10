set(TARGET testvve)

set(SOURCE testvve.cpp)

set(HEADERS )

add_executable(${TARGET} ${SOURCE} ${HEADERS})

target_include_directories(${TARGET} PUBLIC ${INCLUDE})
target_include_directories(${TARGET} PUBLIC $ENV{VULKAN_SDK}/Include)
target_include_directories(${TARGET} PUBLIC $ENV{VULKAN_SDK}/Include/SDL2)
target_include_directories(${TARGET} PUBLIC $ENV{VULKAN_SDK}/Include/Volk)
target_include_directories(${TARGET} PUBLIC $ENV{VULKAN_SDK}/Include/glm)
target_include_directories(${TARGET} PUBLIC $ENV{VULKAN_SDK}/Include/vma)

target_compile_features(${TARGET} PUBLIC cxx_std_20)

target_link_libraries (${TARGET} PUBLIC viennavulkanengine)
target_link_libraries (${TARGET} PUBLIC $ENV{VULKAN_SDK}/lib/SDL2.lib)

message(STATUS ${CMAKE_BUILD_TYPE})

if (MSVC)
	IF(${CMAKE_BUILD_TYPE} STREQUAL "Release")
		message(STATUS "Release mode" )
		target_link_libraries(${TARGET} PUBLIC ${BUILD}/extern/assimp/contrib/zlib/Release/zlibstatic.lib)
		target_link_libraries (${TARGET} PUBLIC ${BUILD}/extern/assimp/lib/Release/assimp-vc143-mt.lib)
	else()
		message(STATUS "Debug mode" ) 
		target_link_libraries(${TARGET} PUBLIC ${BUILD}/extern/assimp/contrib/zlib/Debug/zlibstaticd.lib)
		target_link_libraries (${TARGET} PUBLIC ${BUILD}/extern/assimp/lib/Debug/assimp-vc143-mtd.lib)
	endif()
else()
	IF(${CMAKE_BUILD_TYPE} STREQUAL "Release")
		message(STATUS "Release mode" )
		target_link_libraries(${TARGET} PUBLIC ${BUILD}/extern/assimp/contrib/zlib/zlibstatic.lib)
		target_link_libraries (${TARGET} PUBLIC ${BUILD}/extern/assimp/lib/assimp.lib)
	else()
		message(STATUS "Debug mode" )
		target_link_libraries(${TARGET} PUBLIC ${BUILD}/extern/assimp/contrib/zlib/zlibstaticd.lib)
		target_link_libraries (${TARGET} PUBLIC ${BUILD}/extern/assimp/lib/assimpd.lib)
	endif()
endif()
add_test(NAME testvvetest COMMAND testvve) # Command can be a target
