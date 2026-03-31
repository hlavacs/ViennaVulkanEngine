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

find_package(assimp CONFIG REQUIRED)

target_sources(ViennaVulkanEngine
    PRIVATE
        Engine.cpp
        versions/v3/EngineV3.cpp
        versions/v3/AssetSystem.cpp
        versions/v3/ResourceSystem.cpp
        versions/v3/SceneSystem.cpp
        versions/v3/TaskGraphSystem.cpp
        versions/v3/GraphicsBackend.cpp
        versions/v3/ShaderSystem.cpp
        versions/v3/RenderSystem.cpp
        versions/v3/GuiSystem.cpp
        versions/v3/Runtime.cpp
    PUBLIC
        FILE_SET CXX_MODULES FILES
            ECS.ixx
            System.ixx
            Handle.ixx
            Math.ixx
            Engine.ixx
            versions/v3/V3Types.ixx
            versions/v3/V3Systems.ixx
            versions/v3/EngineV3.ixx
)

target_link_libraries(ViennaVulkanEngine
    PUBLIC
        assimp::assimp
        Vulkan::Vulkan
)

vve_set_output_dirs(ViennaVulkanEngine)
