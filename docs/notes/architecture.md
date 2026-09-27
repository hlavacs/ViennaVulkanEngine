# Engine architecture (after the 2026-09 simplification)

Line counts are rounded and refer to the current tree (simple engine about 7.6k lines; the
simplification took it from 10 409 to 6 781 lines at commit `7cce3e7a`).

## 1. Three layers, one direction

```
application (examples/*, tests/*)            uses only vve::*
   |  import VEEngine;
   v
VEEngine  (src/, namespace vve, ~3.1k lines)                        facade / public contract
   Engine<TSystems...>, World<...>, wrappers AssetSystem . RenderSystem . WindowSystem . GuiSystem (each `Impl &impl_`)
   vocabulary: VEEngine.Types . Math . Error . Handle . Vector . ECSContainer
   |  import VEEngine.Simple;   (only in src/implementations/simple.ixx, the :Implementation partition)
   v
VEEngine.Simple  (src/versions/simple, namespace vve::simple, 7.6k lines)   implementation
   Engine -> ECS . WindowSystem(SDL3) . AssetSystem(assimp) . RenderSystem . GuiSystem(ImGui)
   partitions :Graph :Window :Assets :Gui :RenderSystem + 3 .cpp implementation units
   |
   v
VEEngine.Simple.Renderer  (ForwardRenderer, 1.5k)  ->  VEEngine.Simple.Vulkan  (RAII wrappers + VMA, 2.2k)
   |                                                      :OwnedHandle :Memory :Device :Commands :Presentation
   |                                                      :Pipeline :Shadow :Readback :Resources
   v
VEEngine.Simple.RenderResources . VEEngine.Simple.Scene . VEEngine.Simple.Types     plain CPU data, no Vulkan
```

The import graph is acyclic and strictly downward. The standalone modules (Types, Scene,
RenderResources, Vulkan, Renderer) cannot see `VEEngine.Simple` at all, which lets
`SimpleForwardRendererTests` drive the renderer directly.

The facade binds to an implementation in exactly one place: `src/implementations/<name>.ixx`, the
module partition `VEEngine:Implementation`, selected by the CMake variable
`VVE_ENGINE_IMPLEMENTATION_NAMESPACE` together with `src/versions/<name>/`. That partition imports
the implementation module and exports the aliases `vve::detail::RenderSystemImpl` etc.; every
wrapper holds `Impl &impl_` with `using Impl = detail::<Class>Impl`, and `EngineState` (a
`unique_ptr` with an out-of-line deleter) keeps the engine type out of the exported interface. No
other facade file names an implementation namespace, so a second engine is a new adapter file plus
a new `src/versions/` directory. Only `simple` exists today.

Module unit styles in use:

- `export module X;` / `export module X:Part;` interface units and partitions carry declarations and
  small inline bodies.
- `module VEEngine;`, `module VEEngine.Simple;`, `module VEEngine.Simple.Renderer;` implementation
  units (`src/*.cpp`, `Render/RenderSceneImport.cpp`, `RenderSystemScene.cpp`,
  `RenderSystemObjects.cpp`, `RendererResources.cpp`, `RendererShadowPrep.cpp`, `RendererDraw.cpp`,
  `RendererDebug.cpp`) carry the large member-function definitions and are listed as plain PRIVATE
  sources in CMake.
- `src/implementations/simple.ixx` (`VEEngine:Implementation`) is the facade's only link to the
  implementation module (see above).
- `VEEngine.Simple.Types` is the single vocabulary module of the implementation. `vve::simple` is
  nested in `vve`, so the facade names (Error, Vector, TypedHandle, Transform, ...) are found by
  ordinary lookup; only the math vocabulary (`Vec3`, `add`, `lookAt`, ...) is aliased there. It also
  declares the few types that assets and renderer share (`RenderVertex`, `MaterialTextureSource`,
  `MaterialFactors`, ...).

## 2. The frame

`vve::Engine::step()` -> `simple::Engine::step()` (SDL poll, frame counter, close / frame-cap check)
-> user systems' `update(world, frame, windows)` (detection idiom with `Priority<>` tags: a system
may declare any of three `update` shapes, or none) -> `simple::Engine::renderFrame()` (first call:
create the Vulkan renderer on the first window and bind ImGui to it) -> `RenderSystem::renderFrame`
-> `ForwardRenderer::drawFrame`, which is one linear sequence:

1. `syncSceneResources` uploads only changed meshes, re-uploads the texture slots whose
   `RenderTexture::generation` changed and rebuilds the material buffer when materials or slots
   changed; the swapchain is rebuilt when the window's pixel size no longer matches it.
2. After the in-flight fence wait, `prepareShadowFrame` (CPU) packs the enabled lights and builds all
   110 shadow matrices (spot 0..9, point faces 10..69, directional cascades 70..109) plus the 4
   cascade splits.
3. `FrameUniforms` are written for the current frame in flight; only then is the swapchain image
   acquired, and every early exit after the acquire still consumes its semaphore.
4. `recordCommandBuffer` renders every layer of the three shadow arrays (depth-only pipeline, one
   `shadowVertexMain`, matrix selected by push constant), then the colour pass into an HDR image,
   then the vvppl post-processing chain (or a plain blit when none is set up) into the swapchain
   image, and finally the GUI in its own pass on top.
5. Submit, present.

There is no render graph, no task graph, one pipeline layout, one descriptor set per frame in flight
and a fixed binding table (`shaderBinding` in `Vulkan/Pipeline.ixx`, mirrored by
`[[vk::binding]]` in `simple_forward.slang`). The whole GPU frame is readable top to bottom in
`RendererDraw.cpp`.

## 3. Ownership and errors

Everything is a value member; the only shared ownership is `MaterialTextureSource::embedded`
(`shared_ptr<const EmbeddedImage>`), so the bytes of a texture embedded in a model file are not
copied when the asset catalog hands its texture sources out. `simple::Engine` owns the subsystems,
`RenderSystem` owns the `ForwardRenderer`, the renderer owns every Vulkan object through
`VulkanOwnedHandle` / `VulkanImage` / `VulkanBuffer` (VMA) and `cleanup()` runs in reverse creation
order. Destruction is deterministic; `waitIdle` sits at the one boundary where it matters. The
facade `Engine` is non-copyable and non-movable, so the wrapper references into `EngineState` stay
valid; `simple::RenderSystem` is non-copyable and non-movable too, because the renderer keeps
pointers into its `RenderScene`.

Errors are `std::expected<T, Error>` up to the facade and `VkResult` below `RenderSystem`. The
engine does not throw (Vulkan-Hpp is built with `VULKAN_HPP_NO_EXCEPTIONS`); the vvppl
post-processing library does, and the renderer catches those exceptions and turns them into a
`VkResult`. The one deliberate break in propagation: a failing frame in `drawFrame` is logged
(capped at 16 messages) and skipped rather than propagated, so `run()` keeps going. A failed
swapchain rebuild keeps the device and is retried on the next frame.

## 4. Where the remaining weight is

**Three object models.** The same cube exists as an asset (`AssetMesh` in the catalog), as a
`RenderMesh` / `RenderInstance` in `RenderScene` (`RenderResources.ixx`, 590 lines), and as a
`VulkanMesh` on the GPU. The renderer reads the `RenderScene` meshes, materials, instances and
texture table directly (`bindRenderScene`), so geometry has no second CPU copy; lights still do:
every light is written both into `RenderScene` (`RenderDirectionalLight`, ...) and into the backend
`Scene` (`DirectionalLight`, ...) that shadow preparation and the uniforms read. `RenderSystem`
exists largely to keep these mirrors in step (`render_objects_`, `object_sources_`,
`scene_instances_`, the imported mesh/material caches, the `owner` tag on imported lights and
cameras). The ECS sits in the world but nothing in rendering reads it; cameras and transforms flow
through `RenderSystem` calls, not through entities. Keeping lights only in `RenderScene` would
remove the last backend mirror; whether the ECS drives rendering or leaves the world is a teaching
decision, but one of the two should happen.

**Facade duplication.** Every public method is mirrored 1:1 (`src/RenderSystem.cpp` 291 lines of
one-line forwards, `Assets.cpp` 199, `Window.cpp` 171). The vocabulary in `src/Types.ixx` (about
280 lines) is shared by both layers, not duplicated. The forwarding is the price of an
implementation-independent ABI; it is fine as long as a second implementation is planned,
otherwise it is the largest pure overhead left.

**Infrastructure larger than its use.** `Vector.ixx` (490 lines, a segmented vector) is the biggest
facade file and is used where `std::vector` would do; `Graph.ixx` is a tree used only for the asset
node hierarchy.

**Interface-heavy modules.** Most bodies still live inline in `.ixx` interface units (simple
`Window.ixx` 550, `Assets.ixx` 700, `RenderResources.ixx` 590, all Vulkan wrappers), so editing
one recompiles everything downstream. The `RenderSystem` / `ForwardRenderer` split into `.cpp`
implementation units is the model to extend if incremental build time starts to matter.

**Fixed choices worth knowing.** Camera near / far are constants in `drawFrame` (0.1 / 100) although
`Camera` carries clip planes, and the view uses only the camera position, forward direction and
vertical FOV (the up vector comes from `detail::stableUp`); limits are 64 textures
(`VVE_MAX_SCENE_TEXTURES`; a full table returns `Error::capacity_exceeded`), 10 lights of each
kind, 4 cascades, 1024x1024 shadow maps; `WindowSystem` supports several windows but the renderer
binds the first one only; `RendererId` accepts only `"forward"` (or empty).

## 5. Build notes

- C++23 modules with `import std` (clang >= 22 with libc++ on macOS/Linux, MSVC or clang on
  Windows; CMake >= 3.31, Ninja); vcpkg manifest.
- Clang builds use `-fno-aligned-allocation` (root `CMakeLists.txt`): clang 22 + libc++ `import std`
  does not reliably merge the implicitly declared aligned `operator new(size_t, align_val_t)` with the
  one the std module exports, which made `std::vector` growth "ambiguous" or crashed codegen depending
  on the import graph. Nothing in the engine is over-aligned, so the flag has no runtime effect.
- The white-box tests (`SimpleForwardRendererTests`, `GuiSystemTests` and the GPU tests
  `RenderTextureDedupTests`, `RenderMaterialImportTests`, `RenderLightingCaptureTests`,
  `RenderSponzaCaptureTests`, `RenderMeshDedupTests`) link `ViennaVulkanEngine` and import the
  implementation modules from it; the `simple_modules` file set is PUBLIC for that reason. The ones
  that render pass `vve::simple::Windows` with hidden windows (`WindowDesc{.visible = false}`).
- Shaders are compiled at build time by `slangc` into three SPIR-V files (vertex, fragment, shadow
  vertex); there is no runtime shader reflection.
