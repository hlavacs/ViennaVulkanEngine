# Engine architecture (after the 2026-09 simplification)

Line counts refer to commit `7cce3e7a` (simple engine 6 781 lines, down from 10 409).

## 1. Three layers, one direction

```
application (examples/*, tests/*)            uses only vve::*
   |  import VEEngine;
   v
VEEngine  (src/, namespace vve, ~3.5k lines)                        facade / public contract
   Engine<TSystems...>, World<...>, wrappers AssetSystem . RenderSystem . WindowSystem . GuiSystem (each `Impl &impl_`)
   vocabulary: VEEngine.Types . Math . Error . Handle . Vector . ECSContainer
   |  import VEEngine.Simple;   (only inside src/*.cpp and the facade partitions)
   v
VEEngine.Simple  (src/versions/simple, namespace vve::simple, 6.8k lines)   implementation
   Engine -> ECS . WindowSystem(SDL3) . AssetSystem(assimp) . RenderSystem . GuiSystem(ImGui)
   partitions :Graph :Window :Assets :Gui :RenderSystem :RenderResources + 3 .cpp implementation units
   |
   v
VEEngine.Simple.Renderer  (ForwardRenderer, 1.2k)  ->  VEEngine.Simple.Vulkan  (RAII wrappers + VMA, 2.1k)
   |                                                      :OwnedHandle :Memory :Device :Commands :Presentation
   |                                                      :Pipeline :Shadow :Readback :Resources
   v
VEEngine.Simple.Scene . VEEngine.Simple.Mesh . VEEngine.Simple.Types        plain CPU data, no Vulkan
```

The import graph is acyclic and strictly downward. The standalone modules (Types, Mesh, Scene,
Vulkan, Renderer) cannot see `VEEngine.Simple` at all, which lets `SimpleForwardRendererTests`
drive the renderer directly, and it is what the facade's `VVE_ENGINE_IMPLEMENTATION_NAMESPACE`
switch relies on: the facade only ever names `Impl &`, and `EngineState` (a `unique_ptr` with an
out-of-line deleter) keeps the implementation type out of the exported interface. Only one
implementation exists, so today the switch is a design intent, not a feature.

Module unit styles in use:

- `export module X;` / `export module X:Part;` interface units and partitions carry declarations and
  small inline bodies.
- `module VEEngine;`, `module VEEngine.Simple;`, `module VEEngine.Simple.Renderer;` implementation
  units (`src/*.cpp`, `Render/RenderSceneImport.cpp`, `RenderSystemScene.cpp`,
  `RenderSystemObjects.cpp`, `RendererResources.cpp`, `RendererShadowPrep.cpp`, `RendererDraw.cpp`,
  `RendererDebug.cpp`) carry the large member-function definitions and are listed as plain PRIVATE
  sources in CMake.
- `VEEngine.Simple.Types` is the single vocabulary module of the implementation. `vve::simple` is
  nested in `vve`, so the facade names (Error, Vector, TypedHandle, Transform, ...) are found by
  ordinary lookup; only the math vocabulary (`Vec3`, `add`, `lookAt`, ...) is aliased there.

## 2. The frame

`vve::Engine::step()` -> `simple::Engine::step()` (SDL poll, frame counter, close / frame-cap check)
-> user systems' `update(world, frame, windows)` (detection idiom with `Priority<>` tags: a system
may declare any of three `update` shapes, or none) -> `simple::Engine::renderFrame()` (first call:
create the Vulkan renderer on the first window and bind ImGui to it) -> `RenderSystem::renderFrame`
-> `ForwardRenderer::drawFrame`, which is one linear sequence:

1. `syncSceneResources` uploads only changed meshes and textures.
2. `prepareShadowFrame` (CPU) packs the enabled lights and builds all 110 shadow matrices
   (spot 0..9, point faces 10..69, directional cascades 70..109) plus the 4 cascade splits.
3. `FrameUniforms` are written for the current frame in flight.
4. `recordCommandBuffer` renders every layer of the three shadow arrays (depth-only pipeline, one
   `shadowVertexMain`, matrix selected by push constant), then the colour pass with the GUI inside it.
5. Submit, present.

There is no render graph, no task graph, one pipeline layout, one descriptor set per frame in flight
and a fixed binding table (`shaderBinding` in `Vulkan/Pipeline.ixx`, mirrored by
`[[vk::binding]]` in `simple_forward.slang`). The whole GPU frame is readable top to bottom in
`RendererDraw.cpp`.

## 3. Ownership and errors

Everything is a value member; there is no shared ownership. `simple::Engine` owns the subsystems,
`RenderSystem` owns the `ForwardRenderer`, the renderer owns every Vulkan object through
`VulkanOwnedHandle` / `VulkanImage` / `VulkanBuffer` (VMA) and `cleanup()` runs in reverse creation
order. Destruction is deterministic; `waitIdle` sits at the one boundary where it matters. The
facade `Engine` is non-copyable and non-movable, so the wrapper references into `EngineState` stay
valid.

Errors are `std::expected<T, Error>` up to the facade and `VkResult` below `RenderSystem`;
exceptions are off (`VULKAN_HPP_NO_EXCEPTIONS`). The one deliberate exception: a failing frame in
`drawFrame` is logged (capped at 16 messages) and skipped rather than propagated, so `run()` keeps
going.

## 4. Where the remaining weight is

**Three object models.** The same cube exists as an asset (`AssetMesh` in the catalog), as a
`RenderMesh` / `RenderInstance` in `RenderScene` (`RenderResources.ixx`, 405 lines), as a backend
`Object{Mesh, model, ...}` in `ForwardRenderer::scene`, and as a `VulkanMesh` on the GPU.
`RenderSystem` exists largely to keep these mirrors in step (`render_objects_`, `object_sources_`,
`scene_instances_`, backend index fix-ups on removal). The ECS sits in the world but nothing in
rendering reads it; cameras and transforms flow through `RenderSystem` calls, not through entities.
Collapsing `RenderScene` into the backend `Scene` (keeping only handle -> index maps) would remove
most of `RenderResources.ixx` and a third of `RenderSystem*`; whether the ECS drives rendering or
leaves the world is a teaching decision, but one of the two should happen.

**Facade duplication.** Every public method is mirrored 1:1 (`src/RenderSystem.cpp` 286 lines of
one-line forwards, `Assets.cpp` 200), and `src/Types.ixx` (635 lines) re-declares every descriptor.
That is the price of an implementation-independent ABI; it is fine as long as a second
implementation is planned, otherwise it is the largest pure overhead left.

**Infrastructure larger than its use.** `Vector.ixx` (550 lines, a segmented vector) is the single
biggest file and is used where `std::vector` would do; `Graph.ixx` is a tree used only for the asset
node hierarchy.

**Interface-heavy modules.** Most bodies still live inline in `.ixx` interface units (simple
`Window.ixx` 580, `Assets.ixx` 578, all Vulkan wrappers), so editing one recompiles everything
downstream. The `RenderSystem` / `ForwardRenderer` split into `.cpp` implementation units is the
model to extend if incremental build time starts to matter.

**Fixed choices worth knowing.** Camera FOV / near / far are constants in `drawFrame` although the
scene carries camera data; limits are 8 textures, 10 lights of each kind, 4 cascades, 1024x1024
shadow maps; `WindowSystem` supports several windows but the renderer binds the first one only;
`RendererId` accepts only `"forward"`.

## 5. Build notes

- C++23 modules with `import std` (clang >= 22 with libc++, CMake >= 3.31, Ninja); vcpkg manifest.
- Clang builds use `-fno-aligned-allocation` (root `CMakeLists.txt`): clang 22 + libc++ `import std`
  does not reliably merge the implicitly declared aligned `operator new(size_t, align_val_t)` with the
  one the std module exports, which made `std::vector` growth "ambiguous" or crashed codegen depending
  on the import graph. Nothing in the engine is over-aligned, so the flag has no runtime effect.
- The white-box tests (`SimpleForwardRendererTests`, `GuiSystemTests`) link `ViennaVulkanEngine` and
  import the implementation modules from it; the `simple_modules` file set is PUBLIC for that reason.
- Shaders are compiled at build time by `slangc` into three SPIR-V files (vertex, fragment, shadow
  vertex); there is no runtime shader reflection.
