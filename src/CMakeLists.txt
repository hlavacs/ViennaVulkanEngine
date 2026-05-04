add_library(ViennaVulkanEngine SHARED)
add_library(ViennaVulkanEngine::ViennaVulkanEngine ALIAS ViennaVulkanEngine)

target_compile_features(ViennaVulkanEngine PUBLIC cxx_std_23)
target_compile_definitions(ViennaVulkanEngine PRIVATE VVE_ENGINE_BUILD)

option(VVE_MATH_USE_DOUBLE "Build the engine math layer with double precision" OFF)
if(VVE_MATH_USE_DOUBLE)
    target_compile_definitions(ViennaVulkanEngine PUBLIC VVE_MATH_USE_DOUBLE)
endif()

set(VVE_DEFAULT_VULKAN_ICD "" CACHE STRING "Default Vulkan ICD selector used when VVE_VULKAN_ICD is unset")
set_property(CACHE VVE_DEFAULT_VULKAN_ICD PROPERTY STRINGS "" "system" "moltenvk" "kosmickrisp")
target_compile_definitions(ViennaVulkanEngine PRIVATE VVE_DEFAULT_VULKAN_ICD="${VVE_DEFAULT_VULKAN_ICD}")

set(VVE_ENGINE_IMPLEMENTATION_NAMESPACE "v4" CACHE STRING "Engine implementation namespace exposed by the facade")
set_property(CACHE VVE_ENGINE_IMPLEMENTATION_NAMESPACE PROPERTY STRINGS "v4")
target_compile_definitions(ViennaVulkanEngine
    PUBLIC VVE_ENGINE_IMPLEMENTATION_NAMESPACE=${VVE_ENGINE_IMPLEMENTATION_NAMESPACE})

find_package(assimp CONFIG REQUIRED)
find_package(SDL3 CONFIG REQUIRED)
if(NOT VVE_VULKAN_SDK_ROOT STREQUAL "" AND EXISTS "${VVE_VULKAN_SDK_ROOT}/include/glm/glm.hpp")
   target_include_directories(ViennaVulkanEngine BEFORE PUBLIC "${VVE_VULKAN_SDK_ROOT}/include")
endif()

if(NOT VVE_VULKAN_SDK_ROOT STREQUAL "")
   file(TO_CMAKE_PATH "${VVE_VULKAN_SDK_ROOT}" VVE_VULKAN_SDK_ROOT_COMPILE_PATH)
   target_compile_definitions(ViennaVulkanEngine PRIVATE VVE_VULKAN_SDK_ROOT="${VVE_VULKAN_SDK_ROOT_COMPILE_PATH}")
endif()

if(APPLE AND DEFINED Vulkan_LIBRARY AND NOT Vulkan_LIBRARY STREQUAL "")
   file(TO_CMAKE_PATH "${Vulkan_LIBRARY}" VVE_SDL_VULKAN_LIBRARY_COMPILE_PATH)
   target_compile_definitions(ViennaVulkanEngine
      PRIVATE VVE_SDL_VULKAN_LIBRARY="${VVE_SDL_VULKAN_LIBRARY_COMPILE_PATH}")
endif()

target_sources(ViennaVulkanEngine
    PUBLIC
        FILE_SET CXX_MODULES FILES
            versions/v4/Error.ixx
            versions/v4/Math.ixx
            Math.ixx
            Error.ixx
            versions/v4/Graph.ixx
            Types.ixx
            Graph.ixx
            ECS.ixx
            Window.ixx
            World.ixx
            Engine.ixx
            versions/v4/Vector.ixx
            versions/v4/ECS.ixx
            versions/v4/Types.ixx
            versions/v4/World.ixx
            versions/v4/Assets.ixx
            versions/v4/Resources.ixx
            versions/v4/Shaders.ixx
            versions/v4/Renderer.ixx
            versions/v4/Gui.ixx
            versions/v4/WindowSystem.ixx
            versions/v4/Engine.ixx
)

target_link_libraries(ViennaVulkanEngine
    PUBLIC
        assimp::assimp
    PRIVATE
        SDL3::SDL3
)

vve_set_output_dirs(ViennaVulkanEngine)
