add_library(ViennaVulkanEngine SHARED)
add_library(ViennaVulkanEngine::ViennaVulkanEngine ALIAS ViennaVulkanEngine)

target_compile_features(ViennaVulkanEngine PUBLIC cxx_std_23)
target_compile_definitions(ViennaVulkanEngine PRIVATE VVE_ENGINE_BUILD)
target_compile_options(ViennaVulkanEngine PUBLIC
    "$<$<CXX_COMPILER_ID:MSVC>:/std:c++latest>"
    "$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-std=c++2c>"
)

option(VVE_MATH_USE_DOUBLE "Build the engine math layer with double precision" OFF)
if(VVE_MATH_USE_DOUBLE)
    target_compile_definitions(ViennaVulkanEngine PUBLIC VVE_MATH_USE_DOUBLE)
endif()

set(VVE_DEFAULT_ENGINE_NAMESPACE "v3" CACHE STRING "Default engine namespace selected by the public vve::Engine alias")
set_property(CACHE VVE_DEFAULT_ENGINE_NAMESPACE PROPERTY STRINGS "v3")
target_compile_definitions(ViennaVulkanEngine PUBLIC VVE_DEFAULT_ENGINE_NAMESPACE=${VVE_DEFAULT_ENGINE_NAMESPACE})

set(VVE_DEFAULT_ECS_NAMESPACE "v3" CACHE STRING "Default ECS namespace selected by the public vve::ECS alias")
set_property(CACHE VVE_DEFAULT_ECS_NAMESPACE PROPERTY STRINGS "v3")
target_compile_definitions(ViennaVulkanEngine PUBLIC VVE_DEFAULT_ECS_NAMESPACE=${VVE_DEFAULT_ECS_NAMESPACE})

find_package(assimp CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(SDL3_mixer CONFIG REQUIRED)

target_sources(ViennaVulkanEngine
    PRIVATE
        Engine.cpp
        versions/v3/Engine.cpp
        versions/v3/AssetSystem.cpp
        versions/v3/ResourceSystem.cpp
        versions/v3/SceneSystem.cpp
        versions/v3/TaskGraphSystem.cpp
        versions/v3/WindowSystem.cpp
        versions/v3/GraphicsBackend.cpp
        versions/v3/ShaderSystem.cpp
        versions/v3/RenderSystem.cpp
        versions/v3/GuiSystem.cpp
        versions/v3/Runtime.cpp
    PUBLIC
        FILE_SET CXX_MODULES FILES
            ECS.ixx
            Handle.ixx
            Math.ixx
            Error.ixx
            Engine.ixx
            versions/v3/Types.ixx
            versions/v3/Systems.ixx
            versions/v3/Internal.ixx
            versions/v3/Engine.ixx
)

target_link_libraries(ViennaVulkanEngine
    PUBLIC
        assimp::assimp
        imgui::imgui
        Vulkan::Vulkan
    PRIVATE
        SDL3::SDL3
        SDL3_mixer::SDL3_mixer
)

vve_set_output_dirs(ViennaVulkanEngine)
