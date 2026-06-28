add_library(ViennaVulkanEngine SHARED)
add_library(ViennaVulkanEngine::ViennaVulkanEngine ALIAS ViennaVulkanEngine)

if(WIN32)
    set_target_properties(ViennaVulkanEngine PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
endif()

target_compile_features(ViennaVulkanEngine PUBLIC cxx_std_23)
target_compile_definitions(ViennaVulkanEngine PUBLIC VULKAN_HPP_NO_EXCEPTIONS)
target_compile_definitions(ViennaVulkanEngine PRIVATE VVE_ENGINE_BUILD)

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

set(VVE_ENGINE_IMPLEMENTATION_NAMESPACE "v5" CACHE STRING "Engine implementation namespace exposed by the facade")
set_property(CACHE VVE_ENGINE_IMPLEMENTATION_NAMESPACE PROPERTY STRINGS "v4" "v5")
option(VVE_BUILD_ENGINE_V4 "Build the legacy v4 implementation beside the selected engine" OFF)
if(NOT VVE_ENGINE_IMPLEMENTATION_NAMESPACE STREQUAL "v4" AND
   NOT VVE_ENGINE_IMPLEMENTATION_NAMESPACE STREQUAL "v5")
    message(FATAL_ERROR "VVE_ENGINE_IMPLEMENTATION_NAMESPACE must be v4 or v5")
endif()
target_compile_definitions(ViennaVulkanEngine
    PUBLIC VVE_ENGINE_IMPLEMENTATION_NAMESPACE=${VVE_ENGINE_IMPLEMENTATION_NAMESPACE})

if(VVE_ENGINE_IMPLEMENTATION_NAMESPACE STREQUAL "v5")
    target_compile_definitions(ViennaVulkanEngine PUBLIC VVE_ENGINE_IMPLEMENTATION_IS_V5)
else()
    target_compile_definitions(ViennaVulkanEngine PUBLIC VVE_ENGINE_IMPLEMENTATION_IS_V4)
endif()

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
            Math.ixx
            Error.ixx
            Handle.ixx
            Vector.ixx
            Types.ixx
            ECS.ixx
            Window.ixx
            World.ixx
            Assets.ixx
            RenderSystem.ixx
            Gui.ixx
            Engine.ixx
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

add_subdirectory("versions/v5")
target_compile_definitions(ViennaVulkanEngine PUBLIC VVE_ENGINE_HAS_V5)
add_subdirectory("versions/simple")

if(VVE_BUILD_ENGINE_V4 OR VVE_ENGINE_IMPLEMENTATION_NAMESPACE STREQUAL "v4")
    add_subdirectory("versions/v4")
    target_compile_definitions(ViennaVulkanEngine PUBLIC VVE_ENGINE_HAS_V4)
endif()

target_link_libraries(ViennaVulkanEngine
    PUBLIC
        assimp::assimp
        slang::slang
    PRIVATE
        SDL3::SDL3
        Vulkan::Vulkan
)

vve_set_output_dirs(ViennaVulkanEngine)
