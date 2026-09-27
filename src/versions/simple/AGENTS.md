# AGENTS.md

## src/versions/simple

The simple engine should be the bare engine minimum. Just to render objects, and have lights and shadows. 
- Use a simple scene graph. Do not use render or task graphs.
- Use the existing helpers instead of reimplementing them: Handle, Vector, ECS, Math and the strong types come from the facade vocabulary modules (re-exported through Types.ixx), Graph and Tree are local (Graph.ixx).
- Create empty source files as soon as possible.
- No virtual layer, keep things explicit for the time being.
- Keep the code as simple as possible. 
- Always prefer STL, be it containers, algorithms, etc. Use STL instead of new classes if possible.
- Keep number of new types, structs at a minumum.
- The best class is the class not needed. The same is true for structs, types, enums, functions, etc.
- The engine should mirror the official Vulkan tutorial https://github.com/KhronosGroup/Vulkan-Tutorial (there base the renderer on en/16_Multiple_Objects) in structure.
- It should use SDL3, VMA, Assimp, Slang, dynamic rendering, Vulkan profiles. (Current code: SDL3, VMA, Assimp, stb_image, Slang, dynamic rendering and vvppl for post-processing; Vulkan profiles are not used yet, Vulkan/Device.ixx checks the required queue family and features directly.)
- It should enable loadig assets and rendering multiple objects.
- Basis is Specification 1.4 (the instance requests VK_API_VERSION_1_4).
- It should provide a simple, minimal debugging layer that transports debugging information for Slang, SDL3 windows, rendering output. 
- It should use the light shadow debug example behavior that produces known output that can be stored in a PNG for automatic analysis.
- The debugging example light shadow debug should not be part of the simple engine, but be in the examples folder. It is examples/light_shadow_debug; CTest runs it as LightShadowDebugExample, and it writes bin/<variant>/verify/light_shadow_debug.txt and light_shadow_debug.png.
- The LLM should ingest this output and analyse whether the current version produces the correct result.
- The goal is to let an LLM do the heavy lifiting, to the LLM should be able to reliably detect bugs through the debuggin output.
- Debugging information that is collected and moved e.g. from GPU to CPU should be kept at a minimum necessary. 
- Add code comments and function and class headers. 
- At the start of each file provide an overviw over the functional objects (classes, enums, types, structs, ...) and their purpose in this file.
- The engine should not use Python anywhere, also not for testign. It can use cmd or bash scripts.
- The Renderer should be a simple forward renderer. As Vulkan render path and Slang expample vertex and pixel shaders, use the Vulkan Tutorial and adapt if needed.

## Layout

- Types.ixx (VEEngine.Simple.Types): math aliases, RenderVertex (48 bytes; a zero normal makes the shader use the face normal, a zero tangent skips normal mapping), MaterialTextureSemantic, EmbeddedImage, MaterialFactors. Re-exports VEEngine.Types and VEEngine.ECSContainer.
- Graph.ixx (:Graph): named DAG and Tree helpers used by the asset scene tree.
- Window.ixx (:Window): WindowDesc/Windows, InputState, SDL3 WindowSystem. `visible = false` creates a hidden window that still renders; extents are drawable pixels.
- Assets.ixx (:Assets): Assimp-backed AssetSystem with its Catalog (scenes, nodes, meshes, materials with factors, file or embedded textures, lights and cameras in world space).
- GUI.ixx (:Gui): Dear ImGui context with the SDL3 and Vulkan backends. `processEvent` returns true when ImGui claims a key-down, text or mouse event, which then does not reach InputState (key-up events always do).
- Engine.ixx (VEEngine.Simple): simple::Engine owns ECS, windows, assets, render system and GUI. Options: EngineConfig, ApplicationName, MaxFrames, vve::simple::Windows; any other type is a compile error.
- Scene.ixx (VEEngine.Simple.Scene): RenderTexture, CPU light structs, and the limits from shaders/simple_shared.h (for example 64 texture slots).
- Render/RenderResources.ixx: RenderScene, the CPU meshes, materials, instances, lights, cameras and the texture table (a full table returns Error::capacity_exceeded, purgeUnusedAssets frees slots for reuse).
- Render/RenderSystem.ixx with RenderSystemObjects.cpp, RenderSystemScene.cpp and RenderSceneImport.cpp: simple::RenderSystem (objects, scene instances, lights, cameras, purging, mirroring into the renderer).
- Render/Renderer.ixx with RendererResources.cpp, RendererShadowPrep.cpp, RendererDraw.cpp and RendererDebug.cpp: ForwardRenderer (Vulkan bring-up and resource sync, shadow preparation, frame recording and present, shadow-depth samples and PNG capture).
- Vulkan/*.ixx (VEEngine.Simple.Vulkan partitions): Device, Presentation, Commands, Memory (VMA), Pipeline, Shadow, Resources, Readback, Handle.
- shaders/simple_forward.slang and shaders/simple_shared.h: forward vertex, fragment and shadow vertex stages, compiled to SPIR-V by slangc during the build (target vve_simple_shaders).

## Tests

- White-box tests import VEEngine.Simple directly and link Vulkan::Vulkan (see tests/CMakeLists.txt). With simple::Engine pass vve::simple::Windows, not vve::WindowSetups; use WindowDesc{.visible = false} for hidden windows that still render.
