# Windows: STATIC avoids C++20-module symbol export issues with the MSVC toolchain.
# Linux/macOS: SHARED, whose default-visibility exports need no dllexport annotations.
if(WIN32)
    add_library(ViennaVulkanEngine STATIC)
else()
    add_library(ViennaVulkanEngine SHARED)
endif()
add_library(ViennaVulkanEngine::ViennaVulkanEngine ALIAS ViennaVulkanEngine)

target_compile_features(ViennaVulkanEngine PUBLIC cxx_std_23)
target_compile_definitions(ViennaVulkanEngine PUBLIC VULKAN_HPP_NO_EXCEPTIONS)

option(VVE_MATH_USE_DOUBLE "Build the engine math layer with double precision" OFF)
if(VVE_MATH_USE_DOUBLE)
    target_compile_definitions(ViennaVulkanEngine PUBLIC VVE_MATH_USE_DOUBLE)
endif()

set(VVE_DEFAULT_VULKAN_ICD "" CACHE STRING "Default Vulkan ICD selector used when VVE_VULKAN_ICD is unset")
set_property(CACHE VVE_DEFAULT_VULKAN_ICD PROPERTY STRINGS "" "system" "moltenvk" "kosmickrisp")
target_compile_definitions(ViennaVulkanEngine PRIVATE VVE_DEFAULT_VULKAN_ICD="${VVE_DEFAULT_VULKAN_ICD}")
if(NOT VVE_GENERATED_MOLTENVK_ICD_MANIFEST STREQUAL "")
   file(TO_CMAKE_PATH "${VVE_GENERATED_MOLTENVK_ICD_MANIFEST}" VVE_MOLTENVK_ICD_MANIFEST_COMPILE_PATH)
   target_compile_definitions(ViennaVulkanEngine
      PRIVATE VVE_MOLTENVK_ICD_MANIFEST="${VVE_MOLTENVK_ICD_MANIFEST_COMPILE_PATH}")
endif()
if(NOT VVE_GENERATED_KOSMICKRISP_ICD_MANIFEST STREQUAL "")
   file(TO_CMAKE_PATH "${VVE_GENERATED_KOSMICKRISP_ICD_MANIFEST}" VVE_KOSMICKRISP_ICD_MANIFEST_COMPILE_PATH)
   target_compile_definitions(ViennaVulkanEngine
      PRIVATE VVE_KOSMICKRISP_ICD_MANIFEST="${VVE_KOSMICKRISP_ICD_MANIFEST_COMPILE_PATH}")
endif()

# The facade binds to one implementation through src/implementations/<name>.ixx (module partition
# VEEngine:Implementation) plus the module set in src/versions/<name>/. Nothing else in src/ names it.
set(VVE_ENGINE_IMPLEMENTATION_NAMESPACE "simple" CACHE STRING "Engine implementation compiled into the facade")
set_property(CACHE VVE_ENGINE_IMPLEMENTATION_NAMESPACE PROPERTY STRINGS "simple")
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/implementations/${VVE_ENGINE_IMPLEMENTATION_NAMESPACE}.ixx"
   OR NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/versions/${VVE_ENGINE_IMPLEMENTATION_NAMESPACE}/CMakeLists.txt")
    message(FATAL_ERROR "Unknown engine implementation '${VVE_ENGINE_IMPLEMENTATION_NAMESPACE}': "
        "expected src/implementations/${VVE_ENGINE_IMPLEMENTATION_NAMESPACE}.ixx and src/versions/${VVE_ENGINE_IMPLEMENTATION_NAMESPACE}/.")
endif()

find_package(assimp CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(SDL3 CONFIG REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
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
            Math.ixx
            Error.ixx
            Handle.ixx
            Entity.ixx
            Vector.ixx
            Types.ixx
            ECS.ixx
            ECSContainer.ixx
            Window.ixx
            World.ixx
            Assets.ixx
            RenderSystem.ixx
            Gui.ixx
            Engine.ixx
            implementations/${VVE_ENGINE_IMPLEMENTATION_NAMESPACE}.ixx
    PRIVATE
        Assets.cpp
        Engine.cpp
        Gui.cpp
        RenderSystem.cpp
        Window.cpp
)

if(MSVC)
    set(VVE_MSVC_STD_MODULE_COMPAT_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/MsvcStdModuleCompat.cpp")
    file(GENERATE OUTPUT "${VVE_MSVC_STD_MODULE_COMPAT_SOURCE}" CONTENT [[
#if defined(_MSC_VER)
extern "C" int _Avx2WmemEnabled = 0;
#endif
]])
    target_sources(ViennaVulkanEngine INTERFACE "${VVE_MSVC_STD_MODULE_COMPAT_SOURCE}")
endif()

add_subdirectory("versions/${VVE_ENGINE_IMPLEMENTATION_NAMESPACE}")

target_link_libraries(ViennaVulkanEngine
    PUBLIC
        assimp::assimp
        glm::glm
    PRIVATE
        imgui::imgui
        SDL3::SDL3
        Vulkan::Vulkan
        GPUOpen::VulkanMemoryAllocator
)

vve_set_output_dirs(ViennaVulkanEngine)
