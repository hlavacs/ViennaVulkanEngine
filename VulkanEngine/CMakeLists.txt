cmake_minimum_required(VERSION 3.10)
project(vienna_vulkan_engine)

set(EXECUTABLE_NAME vve_game)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules/")
set(CMAKE_CXX_STANDARD 17)

FILE(GLOB HEADER_FILES
        ${CMAKE_SOURCE_DIR}/*.h
        ${CMAKE_SOURCE_DIR}/*.inl)
FILE(GLOB SOURCE_FILES
        ${CMAKE_SOURCE_DIR}/*.cpp
        ${CMAKE_SOURCE_DIR}/../external/threadpool/ThreadPool.cpp)

add_executable(${EXECUTABLE_NAME} ${HEADER_FILES} ${SOURCE_FILES})


#additional include directories
include_directories(${CMAKE_SOURCE_DIR}/../external/glm)
include_directories(${CMAKE_SOURCE_DIR}/../external/stb)
include_directories(${CMAKE_SOURCE_DIR}/../external/threadpool)
include_directories(${CMAKE_SOURCE_DIR}/../external/nuklear)


# list of all dependencies here
find_package(Vulkan REQUIRED)
include_directories(${Vulkan_INCLUDE_DIRS})
target_link_libraries(${EXECUTABLE_NAME} ${Vulkan_LIBRARY})

set(IRRKLANG_ROOT ${CMAKE_SOURCE_DIR}/../external/irrKlang)
find_package(IrrKlang REQUIRED)
include_directories(${IRRKLANG_INCLUDE_DIR})
target_link_libraries(${EXECUTABLE_NAME} ${IRRKLANG_LIBRARY})

cmake_policy(SET CMP0074 NEW)

if (MSVC)
    add_definitions(-DUNICODE -D_UNICODE)
    set(GLFW3_ROOT ${CMAKE_SOURCE_DIR}/../external/glfw/glfw-3.2.1.bin.WIN64/)
    set(ASSIMP_ROOT_DIR ${CMAKE_SOURCE_DIR}/../external/Assimp)
endif()

find_package(GLFW3 REQUIRED)
include_directories(${GLFW3_INCLUDE_DIR})
target_link_libraries(${EXECUTABLE_NAME} ${GLFW3_LIBRARY})

find_package(ASSIMP REQUIRED)
include_directories(${ASSIMP_INCLUDE_DIRS})
target_link_libraries(${EXECUTABLE_NAME} ${ASSIMP_LIBRARY})

if(UNIX)
    target_link_libraries(${EXECUTABLE_NAME} pthread ${CMAKE_DL_LIBS})
endif()