cmake_minimum_required(VERSION 3.10)
project(vienna_vulkan_engine_cmake)

set(CMAKE_CXX_STANDARD 14)

add_executable(game
        main.cpp
        VEEngine.h
        VEEngine.cpp
        VEEntity.h
        VEEntity.cpp
        VEEventListenerGLFW.h
        VEEventListenerGLFW.cpp
        VEEventListener.h
        VEEventListener.cpp
        VEInclude.h
        VENamedClass.h
        VENamedClass.cpp
        VERenderer.h
        VERenderer.cpp
        VERendererForward.h
        VERendererForward.cpp
        VESceneManager.h
        VESceneManager.cpp
        VESubrender.h
        VESubrender.cpp
        VESubrenderFW_C1.h
        VESubrenderFW_C1.cpp
        VESubrenderFW_Cubemap.h
        VESubrenderFW_Cubemap.cpp
        VESubrenderFW_D.h
        VESubrenderFW_D.cpp
        VESubrenderFW_DN.h
        VESubrenderFW_DN.cpp
        VESubrenderFW_Shadow.h
        VESubrenderFW_Shadow.cpp
        VEWindow.h
        VEWindow.cpp
        VEWindowGLFW.h
        VEWindowGLFW.cpp
        VHBuffer.cpp
        VHCommand.cpp
        VHDebug.cpp
        VHDevice.cpp
        VHFile.cpp
        VHHelper.h
        VHMemory.cpp
        VHRender.cpp
        VHSwapchain.cpp
        vk_mem_alloc.h
)

target_link_libraries(game vulkan glfw assimp pthread)
