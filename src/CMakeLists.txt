add_subdirectory(versions/v3/vh)

add_library(ViennaVulkanEngine SHARED)
add_library(ViennaVulkanEngine::ViennaVulkanEngine ALIAS ViennaVulkanEngine)

target_compile_features(ViennaVulkanEngine PUBLIC cxx_std_23)
target_compile_definitions(ViennaVulkanEngine PRIVATE VVE_ENGINE_BUILD)

option(VVE_MATH_USE_DOUBLE "Build the engine math layer with double precision" OFF)
if(VVE_MATH_USE_DOUBLE)
    target_compile_definitions(ViennaVulkanEngine PUBLIC VVE_MATH_USE_DOUBLE)
endif()

set(VVE_DEFAULT_ENGINE_NAMESPACE "v3" CACHE STRING "Default engine namespace selected by the public vve::Engine alias")
set_property(CACHE VVE_DEFAULT_ENGINE_NAMESPACE PROPERTY STRINGS "v3")
target_compile_definitions(ViennaVulkanEngine PUBLIC VVE_DEFAULT_ENGINE_NAMESPACE=${VVE_DEFAULT_ENGINE_NAMESPACE})

set(VVE_DEFAULT_VULKAN_ICD "" CACHE STRING "Default Vulkan ICD selector used when VVE_VULKAN_ICD is unset")
set_property(CACHE VVE_DEFAULT_VULKAN_ICD PROPERTY STRINGS "" "system" "moltenvk" "kosmickrisp")
target_compile_definitions(ViennaVulkanEngine PRIVATE VVE_DEFAULT_VULKAN_ICD="${VVE_DEFAULT_VULKAN_ICD}")

find_package(assimp CONFIG REQUIRED)
find_package(SDL3 CONFIG REQUIRED)
find_package(Stb REQUIRED)

if(NOT VVE_VULKAN_SDK_ROOT STREQUAL "" AND EXISTS "${VVE_VULKAN_SDK_ROOT}/include/glm/glm.hpp")
   target_include_directories(ViennaVulkanEngine BEFORE PUBLIC "${VVE_VULKAN_SDK_ROOT}/include")
endif()
target_include_directories(ViennaVulkanEngine PRIVATE "${Stb_INCLUDE_DIR}")

if(NOT VVE_VULKAN_SDK_ROOT STREQUAL "")
   file(TO_CMAKE_PATH "${VVE_VULKAN_SDK_ROOT}" VVE_VULKAN_SDK_ROOT_COMPILE_PATH)
   target_compile_definitions(ViennaVulkanEngine PRIVATE VVE_VULKAN_SDK_ROOT="${VVE_VULKAN_SDK_ROOT_COMPILE_PATH}")
endif()

if(APPLE AND DEFINED Vulkan_LIBRARY AND NOT Vulkan_LIBRARY STREQUAL "")
   file(TO_CMAKE_PATH "${Vulkan_LIBRARY}" VVE_SDL_VULKAN_LIBRARY_COMPILE_PATH)
   target_compile_definitions(ViennaVulkanEngine PRIVATE VVE_SDL_VULKAN_LIBRARY="${VVE_SDL_VULKAN_LIBRARY_COMPILE_PATH}")
endif()

target_sources(ViennaVulkanEngine
    PRIVATE
        EngineFacade.cpp
        versions/v3/EngineV3.cpp
        versions/v3/AssetSystem.cpp
        versions/v3/ResourceSystem.cpp
        versions/v3/SceneSystem.cpp
        versions/v3/SceneLoader.cpp
        versions/v3/TaskGraphSystem.cpp
        versions/v3/TaskExecution.cpp
        versions/v3/FrameTasks.cpp
        versions/v3/WindowSystem.cpp
        versions/v3/GraphicsBackend.cpp
        versions/v3/ShaderSystem.cpp
        versions/v3/RenderSystem.cpp
        versions/v3/GuiSystem.cpp
        versions/v3/Runtime.cpp
        versions/v3/GraphDump.cpp
    PUBLIC
        FILE_SET CXX_MODULES FILES
            ECS.ixx
            World.ixx
            Handle.ixx
            Math.ixx
            Error.ixx
            Engine.ixx
            versions/v3/Vector.ixx
            versions/v3/Types.ixx
            versions/v3/Systems.ixx
            versions/v3/Internal.ixx
            versions/v3/Engine.ixx
)

target_link_libraries(ViennaVulkanEngine
    PUBLIC
        assimp::assimp
        Vulkan::Vulkan
    PRIVATE
        vh::VulkanHelpers
        slang::slang
        SDL3::SDL3
)

vve_set_output_dirs(ViennaVulkanEngine)
