set(TARGET testvve)

set(SOURCE testvve.cpp)

set(HEADERS )

add_executable(${TARGET} ${SOURCE} ${HEADERS})

target_compile_features(${TARGET} PUBLIC cxx_std_20)

target_link_libraries (${TARGET} PUBLIC viennavulkanengine)
target_link_libraries (${TARGET} PUBLIC ${VULKAN}/lib/SDL2.lib)

message(STATUS ${CMAKE_BUILD_TYPE})

if (MSVC)
	IF(${CMAKE_BUILD_TYPE} STREQUAL "Release")
		message(STATUS "Release mode" )
		target_link_libraries(${TARGET} PUBLIC ${zlib_BINARY_DIR}/zlibstatic.lib)
		target_link_libraries (${TARGET} PUBLIC ${assimp_BINARY_DIR}/Release/assimp-vc143-mt.lib)
	else()
		message(STATUS "Debug mode" ) 
		target_link_libraries(${TARGET} PUBLIC ${zlib_BINARY_DIR}/zlibstaticd.lib)
		target_link_libraries (${TARGET} PUBLIC ${assimp_BINARY_DIR}/Debug/assimp-vc143-mtd.lib)
	endif()
else()
	IF(${CMAKE_BUILD_TYPE} STREQUAL "Release")
		message(STATUS "Release mode" )
		target_link_libraries(${TARGET} PUBLIC ${zlib_BINARY_DIR}/zlibstatic.lib)
		target_link_libraries (${TARGET} PUBLIC ${assimp_BINARY_DIR}/lib/assimp.lib)
	else()
		message(STATUS "Debug mode" )
		target_link_libraries(${TARGET} PUBLIC ${zlib_BINARY_DIR}/zlibstaticd.lib)
		target_link_libraries (${TARGET} PUBLIC ${assimp_BINARY_DIR}/lib/assimpd.lib)
	endif()
endif()
add_test(NAME testvvetest COMMAND testvve) # Command can be a target
