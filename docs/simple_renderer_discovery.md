# Simple Renderer Discovery

Discovery date: 2026-06-28.

## Instructions Re-Read

- `AGENTS.md`: simple is a concrete engine under `src/versions/simple`; concrete engines must stay isolated; simple should be the bare minimum engine.
- `src/versions/simple/AGENTS.md`: simple should render objects with lights and shadows, use a v5-like scene graph but no render/task graphs, reuse v5 helper classes (`Handle`, `Vector`, `Graph`, `ECS`, `Math`), prefer STL, mirror Vulkan Tutorial `en/16_Multiple_Objects`, and use SDL3, VMA, Assimp, Slang, dynamic rendering, Vulkan profiles.

## File Inventory

- `src/versions/simple/AGENTS.md`: local instructions for the simple engine.
- `src/versions/simple/CMakeLists.txt`: defines the `ViennaVulkanEngineSimple` target and `vve_simple_shaders` Slang-to-SPIR-V custom target.
- `src/versions/simple/Assets.ixx`: aliases the v5 asset system and asset-facing types into `vve::simple`.
- `src/versions/simple/ECS.ixx`: aliases v5 `Entity`, `DefaultECSTraits`, `BasicECS`, `ECS`, and `Vector` into `vve::simple`.
- `src/versions/simple/Engine.ixx`: aliases v5 engine configuration/frame types and facade engine factories into `vve::simple`.
- `src/versions/simple/GUI.ixx`: aliases v5 GUI and render-pass contract types into `vve::simple`.
- `src/versions/simple/Graph.ixx`: aliases v5 `Graph<THandle>`, `Tree<THandle>`, `Vector`, `Error`, and `ObjectName` into `vve::simple`.
- `src/versions/simple/Handle.ixx`: aliases v5 `TypedHandle` and `HandleHash`, and forwards handle factory helpers.
- `src/versions/simple/Math.ixx`: aliases v5 math scalar/vector/matrix/quaternion types and math functions into `vve::simple`.
- `src/versions/simple/Mesh.ixx`: defines CPU-side `Vertex`, `Mesh`, and `makeCube()` sample indexed geometry for the simple renderer.
- `src/versions/simple/Renderer.ixx`: defines `vve::simple::Renderer`, which owns Vulkan bring-up wrappers, shader modules, pipeline state, and a CPU `Scene`.
- `src/versions/simple/Scene.ixx`: defines CPU-side `Object`, `Scene`, and `makeSampleScene()` with three cube objects.
- `src/versions/simple/SceneGraph.ixx`: aliases v5 scene graph handle and tree-related types into `vve::simple`.
- `src/versions/simple/Vector.ixx`: aliases v5 `Vector`, iterator aliases, and `VectorConstRange` into `vve::simple`.
- `src/versions/simple/Vulkan.ixx`: defines explicit Vulkan helper structs for instance, surface, device, swapchain, render pass, framebuffers, descriptor layout, pipeline layout, shader modules, graphics pipeline, command pool/buffers, sync, buffers, descriptor pool/sets, mesh upload, and uniform buffers.
- `src/versions/simple/Window.ixx`: aliases v5 window descriptor/info/frame-data types into `vve::simple`.
- `src/versions/simple/WindowSystem.ixx`: aliases v5 window-system, window, input, and window collection types into `vve::simple`.
- `src/versions/simple/World.ixx`: aliases the facade `World<TObjects...>` plus v5 subsystem and handle types into `vve::simple`.
- `src/versions/simple/shaders/simple_forward.vert.slang`: Slang vertex shader with `vertexMain`, frame uniform buffer, model push constants, and position/color/texcoord vertex input.
- `src/versions/simple/shaders/simple_forward.frag.slang`: Slang fragment shader with `fragmentMain` that writes interpolated vertex color to one color attachment.

## Renderer State

- Current renderer class: `vve::simple::Renderer` in `src/versions/simple/Renderer.ixx`.
- Renderer public methods: `init(SDL_Window*)`, `loadScene(Scene)`, `drawFrame()`, and `cleanup()`.
- `init()` creates a Vulkan instance, SDL surface, physical/logical device, swapchain, image views, render pass, framebuffers, descriptor-set layout, pipeline layout, shader modules, and graphics pipeline.
- `drawFrame()` currently performs frame acquisition, uniform update, command recording, queue submit, and presentation, and `recordCommandBuffer()` iterates uploaded meshes against `scene.objects`.
- CPU render data exists as `vve::simple::Scene`, `vve::simple::Object`, `vve::simple::Mesh`, and `vve::simple::Vertex`, using `std::vector` containers.
- GPU upload helpers exist in `Vulkan.ixx`, including `VulkanBuffer`, `VulkanMesh`, `FrameUniforms`, `VulkanUniformBuffers`, `VulkanDescriptorPool`, and `VulkanDescriptorSets`; `Renderer::init()` creates the uniform/descriptor resources and uploads one `VulkanMesh` per current scene object.
- `Renderer::init()` currently loads SPIR-V from `VVE_SIMPLE_SHADER_DIR` with `simple_forward.vert.spv` and `simple_forward.frag.spv`, matching the CMake-generated shader output directory.

## v5 Helper Reuse

- `Handle`: `src/versions/simple/Handle.ixx` uses `export import VEEngine.V5.Handle` and aliases/forwards v5 handle types and helpers.
- `Vector`: `src/versions/simple/Vector.ixx` uses `export import VEEngine.V5.Vector` and aliases v5 vector container/range types.
- `Graph`: `src/versions/simple/Graph.ixx` uses `export import VEEngine.V5` and aliases `vve::v5::Graph<THandle>` and `vve::v5::Tree<THandle>`.
- `ECS`: `src/versions/simple/ECS.ixx` uses `export import VEEngine.V5` and aliases v5 ECS types.
- `Math`: `src/versions/simple/Math.ixx` uses `export import VEEngine.V5.Math` and aliases v5 math types/functions.
- Other simple modules also use `export import VEEngine.V5` or `export import VEEngine` to reuse v5/facade types: `Assets.ixx`, `Engine.ixx`, `GUI.ixx`, `SceneGraph.ixx`, `Window.ixx`, `WindowSystem.ixx`, and `World.ixx`.

## Shader State

- `src/versions/simple/shaders` exists.
- Existing Slang shaders:
  - `src/versions/simple/shaders/simple_forward.vert.slang`
  - `src/versions/simple/shaders/simple_forward.frag.slang`
- No `.spv` files are present under `src/versions/simple` in the source tree.
- `src/versions/simple/CMakeLists.txt` compiles both Slang files to SPIR-V using `-target spirv -profile spirv_1_4`, stages `vertex`/`fragment`, and entries `vertexMain`/`fragmentMain`.

## Build Wiring

- Top-level `CMakeLists.txt` finds Vulkan and Slang. It first tries `find_package(slang CONFIG QUIET)`; if `slang::slang` is missing, it creates an imported `slang::slang` target from the Vulkan SDK fallback.
- `src/CMakeLists.txt` adds `add_subdirectory("versions/simple")` unconditionally after `versions/v5`.
- `src/CMakeLists.txt` currently allows `VVE_ENGINE_IMPLEMENTATION_NAMESPACE` values `v4` or `v5`; `simple` is not currently selectable as the facade implementation namespace there.
- `src/versions/simple/CMakeLists.txt` defines `ViennaVulkanEngineSimple` as `STATIC EXCLUDE_FROM_ALL`, links it to `ViennaVulkanEngine`, and includes all simple `.ixx` files in a C++ module file set.
- `src/versions/simple/CMakeLists.txt` defines `vve_simple_shaders`, which depends on generated `simple_forward.vert.spv` and `simple_forward.frag.spv`; `ViennaVulkanEngineSimple` depends on `vve_simple_shaders`.
- Slang compiler selection in `src/versions/simple/CMakeLists.txt`: `SLANGC_EXECUTABLE`, then `SLANG_EXECUTABLE`, then `slang::slangc`, then `find_program(VVE_SIMPLE_SLANGC NAMES slangc slangc.exe ...)` near `slang::slang` and `VVE_VULKAN_SDK_ROOT`.
- Relevant presets are in `cmake/CMakePresets.base.json`; `debug-macos-arm64-llvm` configures Ninja Debug with the macOS arm64 LLVM toolchain, `VVE_DEFAULT_VULKAN_ICD=kosmickrisp`, `VVE_VCPKG_TRIPLET=arm64-osx`, and inherited `VVE_ENGINE_IMPLEMENTATION_NAMESPACE=v5`. `build-debug-macos-arm64-llvm` builds that configure preset with 16 jobs.

## Verification

- Per task constraint, no full configure or build was run.
- Source, build, and shader files were not modified by this discovery task.

## End-to-end driving (iter 55 review)

- Public simple renderer type: `struct Renderer` is declared in `src/versions/simple/Renderer.ixx:21`.
- Public signatures:
  - `[[nodiscard]] VkResult init(SDL_Window *sdlWindow)` at `src/versions/simple/Renderer.ixx:52`.
  - `void loadScene(Scene nextScene)` at `src/versions/simple/Renderer.ixx:158`.
  - `void drawFrame()` at `src/versions/simple/Renderer.ixx:163`.
  - `void cleanup()` at `src/versions/simple/Renderer.ixx:226`.
- Window ownership: `Renderer` stores only a borrowed `SDL_Window *window` at `src/versions/simple/Renderer.ixx:43`; `init()` receives that borrowed pointer, stores it, rejects null, creates the Vulkan surface from it, and queries its pixel size at `src/versions/simple/Renderer.ixx:52`, `src/versions/simple/Renderer.ixx:58`, `src/versions/simple/Renderer.ixx:64`, and `src/versions/simple/Renderer.ixx:73`.
- Simple engine/window flow is still v5-backed: `src/versions/simple/Engine.ixx:21` aliases `vve::v5::Engine`, while `src/versions/simple/WindowSystem.ixx:15` says SDL lifecycle and window ownership remain implemented by v5. v5 owns `WindowSystem::Impl::windows` at `src/versions/v5/Window.ixx:347`, creates visible `SDL_Window` objects at `src/versions/v5/Window.ixx:370` and `src/versions/v5/Window.ixx:400`, and v5 engine init calls `window_system_.init(windows_)` at `src/versions/v5/Engine.ixx:157`.
- Scene sample entry: `Scene makeSampleScene()` is defined at `src/versions/simple/Scene.ixx:33` and returns three cube objects at `src/versions/simple/Scene.ixx:34`.
- Executable entry points currently wired by CMake are `game`, `physics`, `sponza`, and `light_shadow_debug` at `examples/CMakeLists.txt:1`, `examples/CMakeLists.txt:5`, `examples/CMakeLists.txt:9`, and `examples/CMakeLists.txt:13`. Each links only `ViennaVulkanEngine::ViennaVulkanEngine`, not `ViennaVulkanEngineSimple`, at `examples/game/CMakeLists.txt:5`, `examples/physics/CMakeLists.txt:5`, `examples/sponza/CMakeLists.txt:5`, and `examples/light_shadow_debug/CMakeLists.txt:5`.
- Current executable behavior: `examples/light_shadow_debug/light_shadow_debug.cpp:477` constructs a facade engine and `examples/light_shadow_debug/light_shadow_debug.cpp:489` calls `engine.run()`; `examples/game/game.cpp:291` constructs a facade engine and `examples/game/game.cpp:325` drives `engine.step()` in a loop; `examples/physics/physics.cpp:99` constructs a facade engine and `examples/physics/physics.cpp:116` calls `engine.run()`; `examples/sponza/sponza.cpp:192` constructs a facade engine and `examples/sponza/sponza.cpp:209` calls `engine.run()`.
- Definitive driving status: no executable currently constructs `vve::simple::Renderer`, calls `loadScene(makeSampleScene())`, or calls `drawFrame()` in a loop. The next missing wiring step is an executable or engine integration path that obtains a v5-owned/native SDL window, constructs `vve::simple::Renderer`, calls `loadScene(makeSampleScene())` before `init()`, then drives `drawFrame()` until shutdown before calling `cleanup()`.
