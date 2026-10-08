# Engine architecture (after the 2026-09 simplification)

Line counts are rounded and refer to the current tree (simple engine about 7.6k lines; the
simplification took it from 10 409 to 6 781 lines at commit `7cce3e7a`).

## 1. Three layers, one direction

```
application (examples/*, tests/*)            uses only vve::*
   |  import VVEngine;
   v
VVEngine  (src/, namespace vve, ~3.1k lines)                        facade / public contract
   Engine<TSystems...>, World<...>, wrappers AssetSystem . RenderSystem . WindowSystem . GuiSystem (each `Impl &impl_`)
   vocabulary: VVEngine.Types . Math . Error . Handle . Vector . ECSContainer
   |  import VVEngine.Simple;   (only in src/implementations/simple.ixx, the :Implementation partition)
   v
VVEngine.Simple  (src/versions/simple, namespace vve::simple, 7.6k lines)   implementation
   Engine -> ECS . WindowSystem(SDL3) . AssetSystem(assimp) . RenderSystem . GuiSystem(ImGui)
   partitions :Graph :Window :Assets :Gui :RenderSystem + 3 .cpp implementation units
   |
   v
VVEngine.Simple.Renderer  (ForwardRenderer, 1.5k)  ->  VVEngine.Simple.Vulkan  (RAII wrappers + VMA, 2.2k)
   |                                                      :OwnedHandle :Memory :Device :Commands :Presentation
   |                                                      :Pipeline :Shadow :Readback :Resources
   v
VVEngine.Simple.RenderResources . VVEngine.Simple.Scene . VVEngine.Simple.Types     plain CPU data, no Vulkan
```

The import graph is acyclic and strictly downward. The standalone modules (Types, Scene,
RenderResources, Vulkan, Renderer) cannot see `VVEngine.Simple` at all, which lets
`SimpleForwardRendererTests` drive the renderer directly.

The facade binds to an implementation in exactly one place: `src/implementations/<name>.ixx`, the
module partition `VVEngine:Implementation`, selected by the CMake variable
`VVE_ENGINE_IMPLEMENTATION_NAMESPACE` together with `src/versions/<name>/`. That partition imports
the implementation module and exports the aliases `vve::detail::RenderSystemImpl` etc.; every
subsystem wrapper holds `Impl &impl_` with `using Impl = detail::<Class>Impl`. The facade `Engine`
owns `std::unique_ptr<detail::EngineImpl>` and calls the selected implementation directly; the
inline non-template `detail::makeEngineImpl` factory in `src/Engine.ixx` converts startup options. No
other facade file names an implementation namespace, so a second engine is a new adapter file plus
a new `src/versions/` directory. Only `simple` exists today.

Module unit styles in use:

- `export module X;` / `export module X:Part;` interface units and partitions carry declarations and
  small inline bodies.
- `module VVEngine;`, `module VVEngine.Simple;`, `module VVEngine.Simple.Renderer;` implementation
  units (`src/*.cpp`, `Render/RenderSceneImport.cpp`, `RenderSystemScene.cpp`,
  `RenderSystemObjects.cpp`, `RendererResources.cpp`, `RendererShadowPrep.cpp`, `RendererDraw.cpp`,
  `RendererDebug.cpp`) carry the large member-function definitions and are listed as plain PRIVATE
  sources in CMake.
- `src/implementations/simple.ixx` (`VVEngine:Implementation`) is the facade's only link to the
  implementation module (see above).
- `VVEngine.Simple.Types` is the single vocabulary module of the implementation. `vve::simple` is
  nested in `vve`, so the facade names (Error, Vector, TypedHandle, Transform, ...) are found by
  ordinary lookup; only the math vocabulary (`Vec3`, `add`, `lookAt`, ...) is aliased there. It also
  declares the few types that assets and renderer share (`RenderVertex`, `MaterialTextureSource`,
  `MaterialFactors`, ...).

## 2. The frame

`vve::Engine::step()` -> `simple::Engine::step()` (SDL poll, frame counter, close / frame-cap check)
-> user systems' `update(world, frame, windows)` (detection idiom with `Priority<>` tags: a system
may declare any of three `update` shapes, or none) -> `simple::Engine::renderFrame()` (first call:
create the shared Vulkan device on the first rendered window, add targets in window order and attempt ImGui initialization there) -> `RenderSystem::renderFrame`
-> `ForwardRenderer::drawFrame` for each target, which is one linear sequence:

1. A pixel-size change rebuilds the swapchain using `oldSwapchain`, releasing the old handle
   only after successful replacement. Dynamic viewport and scissor keep the graphics pipelines alive across resizes.
2. The in-flight fence wait is bounded to 1 s; a timeout increments `skippedFrameCount()` and returns
   before changing the frame slot's data or synchronization state. A successful wait advances the
   completed submission serial and reclaims retired owners covered by that fence. `syncSceneResources`
   then uploads changed meshes and texture generations, retiring replaced or removed meshes, buffers
   and textures at the latest submitted serial. Materials and edited vertices have one copy per
   window and frame slot, with dirty bitmasks; only the completed slot is written. Texture descriptors
   update only changed array elements in that slot, including fallback views for released textures.
   `prepareShadowFrame` fills the renderer's `FrameUniforms` directly with camera data, packed lights
   and shadow matrices; `frameUniforms()` exposes the latest window's prepared data. The point-light
   loop uses `activePointLightCount`, replacing the padding word without changing the std140 size.
   `ShadowLightMeta` retains the packed light index/type, view/projection and near/far planes; its
   `first_layer` addresses that type's array (spot: packed index, point: packed index ×6 + face,
   directional: packed index ×4 + cascade), independently of the uniform matrix base.
   Preparation packs enabled lights with positive intensity; spot and directional
   lights with nonpositive intensity remain packed only when ambient is positive. Their direction w
   is 1 (no shadow), while xyz stays normalized; they produce no shadow matrix, metadata or readback.
   Shadow-casting lights build matrices in the fixed uniform ranges (spot 0..9, point faces 10..69,
   directional cascades 70..109), plus the 4 cascade splits. `ensureShadowCapacity` grows each array
   to its packed count times 1, 6 or 4, with a minimum of one layer and no shrinking. Growth retires
   the old images, views and samplers by submission serial and transitions every new layer to
   SHADER_READ_ONLY_OPTIMAL once. Every window's slots are marked pending; the three shadow bindings
   change only after that slot's fence completes, before its next submission. Even one-layer whole-image
   views are 2D arrays, matching the shader samplers.
3. Shadow-depth samples are built only with GPU readback enabled; otherwise the sample list is cleared.
   `FrameUniforms` are written for the current frame in flight, and the GUI-owning window prepares its
   draw data before command recording; only then is the swapchain image
   acquired with a 100 ms timeout. `acquireAction` maps `VK_TIMEOUT`/`VK_NOT_READY` to a silent skip
   counted by `skippedFrameCount()`; these results signal no semaphore. `VK_SUCCESS`/`VK_SUBOPTIMAL_KHR`
   render, `VK_ERROR_OUT_OF_DATE_KHR` recreates, and other results report a failure. Once acquisition
   succeeds, every early exit still consumes its semaphore.
4. `recordCommandBuffer` resolves visible instances once into a `std::vector<DrawItem>` sorted by mesh,
   containing the GPU mesh/vertex buffer, material slot, model, flags and world AABB. `VulkanMesh`
   computes its object-space culling box from vertices at creation and every vertex update; transformed corners enclose rotated
   and nonuniformly scaled instances. Each pass tests its own camera, spot, point-face or directional
   cascade frustum, keeping touching/intersecting and invalid bounds. Pass loops perform no map lookups
   and bind vertex/index buffers only when the mesh changes, including across rendering boundaries.
   The shadow pipeline, descriptor set and viewport bind once before the first shadow layer.
   `lastFrameDrawStats()` reports forward/shadow draws and vertex-buffer/pipeline binds for the last
   recorded window frame, excluding GUI and post-processing. Recording clears and renders only shadow-casting lights' layers (depth-only pipeline,
   one `shadowVertexMain`, matrix selected by push constant); unused and ambient-only layers keep
   their read-only layout. `lastShadowLayerPassCount()` counts actual layer passes in the last recorded
   window frame. The shader's uniform per-light no-shadow branch preserves ambient and skips PCF.
   Recording then continues with the colour pass into an HDR image,
   then the vvppl post-processing chain (or a plain blit when none is set up) into the swapchain
   image, and finally the GUI in its own pass on top when preparation produced vertices, only for the first rendered window. Every target uses its
   own camera uniforms, directional cascades and extent; shared shadow arrays are synchronized between targets.
5. Submit waits for acquire at TRANSFER, leaving shadow and HDR rendering free to proceed. The
   post-forward barrier includes TRANSFER in its source stages so the swapchain layout transition
   follows that wait. With no GUI vertices, the GUI barrier and pass are skipped and the final barrier
   transitions GENERAL to PRESENT_SRC from TRANSFER/TRANSFER_WRITE; with GUI, it transitions from
   COLOR_ATTACHMENT_OPTIMAL using COLOR_ATTACHMENT_OUTPUT/COLOR_ATTACHMENT_WRITE.
   `vkQueuePresentKHR` can still block; the fence/acquire limits do not bound it.
   One-time upload waits in `submitOnce` are unchanged.

`simple::Engine` owns the step count and timestamp and stores the latest `FrameContext` before
incrementing the count used by `MaxFrames`. The first step uses `DeltaTime{}`; later deltas span
consecutive polls, even when a user update or rendering fails. The facade reads this context for
user-system hooks and exposes it through `Engine::frameContext() const` without its own clock or counter.

`simple::Engine::run()` calls `step()` and then `renderFrame()` on each iteration, returning either
call's error. It renders before checking `FrameStatus::stopped`, so the frame that reaches `MaxFrames`
is presented too. `RenderFrameCountTests` checks that a two-frame run presents twice in a hidden window.

`drawFrame` and the renderer's `renderFrame` return true only when presentation returns
`VK_SUCCESS` or `VK_SUBOPTIMAL_KHR`; skipped frames return false and retain the existing swapchain
rebuild behavior. `RenderSystem::renderFrame` requires initialization (`Error::not_initialized`)
and treats a skipped frame as success without advancing its frame or FPS counters. Its last rendered
window count is the number of targets actually presented. Frame and FPS counters advance once when at
least one target presents, and stay unchanged when all targets skip.

`simple::Engine::renderFrame()` checks the minimized flag and live SDL drawable size before
rendering. Minimized windows and windows with zero drawable pixels skip drawing while retaining
their targets, camera overrides, post-processing chains and GUI ownership. When no window can
render, the presented-window count is zero and `SDL_WaitEventTimeout(nullptr, 100)` waits for an
event or timeout without consuming the event; the next poll can process a restore. If all windows
start minimized, lazy GPU initialization is deferred until one can render. `RenderFrameCountTests`
checks one and both windows minimized, unchanged frame/FPS counts when all skip, restoration and
retained resources, including that rendering only the second window does not move the GUI there.

GUI initialization checks SDL3, Vulkan init info, the Vulkan backend and font upload in order.
Only successful backend calls set readiness flags; `GuiSystem::ready()` requires both backends and
fonts. A failed stage logs one `[vve::simple] GUI disabled: <stage>` line, shuts down initialized GUI
parts and leaves the GUI preparation and record sinks unset. Rendering and `step()` continue without the GUI.

On the first rendered window, `VK_KHR_swapchain_mutable_format` swapchain creation lists its sRGB format and compatible
UNORM format in `VkImageFormatListCreateInfo`. `VulkanImageViews` owns both view sets and releases
them before swapchain destruction or recreation. Only the GUI attachment and ImGui pipeline use
UNORM, preserving authored GUI bytes while scene and post-processing blits retain sRGB encoding.
Without the extension, `GuiSystem::initContext` linearizes the initial theme RGB once; custom draw
colours and later theme changes are not corrected. `GuiColorCaptureTests` captures grey 128±2
on the mutable-format path before and after swapchain recreation; `GuiSystemTests` checks the fallback.

`GuiSystem::prepareFrame()` runs NewFrame, saves ImGui's recovery state and invokes the user callback. A
`std::exception` is logged once; ImGui recovers unfinished windows and stacks with assertions
temporarily disabled, then finishes ImGui draw data and returns whether it contains vertices.
`record(cmd)` records that prepared data inside the GUI pass without invoking the callback again.
The next `simple::Engine::step()` consumes the
pending failure and returns `Error::platform_error`; replacing the callback allows later steps
to continue. `GuiCallbackErrorTests` covers this path through a hidden window.

`RenderSystem::setPostProcessSetup` runs its callback once per rendered window when that window's
target is created. Each `vvppl::PostProcessing` reference and its settings references remain valid
until that window closes or the engine shuts down, including across resizes, so applications may keep and change them at any time.
`RecordedPass::post_process` identifies an applied chain and `RecordedPass::gui` an actual GUI pass.
The fetched vvppl v1.0 API has no public empty-chain query, so a configured empty chain still uses
`apply`; bypassing it needs an upstream API and tag.

There is no render graph, no task graph, one pipeline layout, one descriptor set per window and frame in flight
and a fixed binding table (`shaderBinding` in `Vulkan/Pipeline.ixx`, mirrored by
`[[vk::binding]]` in `simple_forward.slang`). The whole GPU frame is readable top to bottom in
`RendererDraw.cpp`.

The material storage-buffer binding is visible only to fragments; vertex outputs carry geometry and
UVs without a colour varying. The uniform per-object `unlit` push constant returns the sampled base
colour before the remaining material samples, lighting loops and shadow comparisons.

## 3. Ownership and errors

`RenderSystem` caches primitive materials by base colour and texture slot, and plane/cuboid meshes by
shape and local extents. Placement stays in each instance's `Transform`; arbitrary triangle meshes
share materials only. `removeObject` releases its mesh and material when no instance references them
and no imported cache owns them, then releases unused texture slots. Imported materialless meshes
share one cached default material. Explicit purge still reclaims unused imported resources, and
cache hits verify that handles remain live. Editing shared geometry clones it; an in-place primitive
edit evicts its cache entry so future objects receive the original shape.

`ForwardRenderer` owns an ordered `std::list<WindowTarget>` so non-movable RAII wrappers retain stable
addresses. The first rendered surface selects the shared device. Further surfaces must support
presentation from its graphics queue; otherwise renderer initialization returns `Error::platform_error`
with a diagnostic. Each target owns
its surface, swapchain (including `requestedExtent`), scene and GUI image views, depth/HDR images,
frame synchronization and submission serials, command buffers, uniform and material buffers,
per-slot dynamic vertex copies, descriptor pool/sets, post-processing
chain, camera and capture state. Drawing, resizing and capture take that target. Shared device,
allocator, pipelines, shadow arrays, scene GPU resources and command pool stay in `ForwardRenderer`;
cleanup releases all targets before those shared objects. Resizes affect only the selected target.
A closed window's target is removed after `vkDeviceWaitIdle`; `anyShouldClose` still stops the engine.
The first rendered window keeps GUI ownership, with input events from all windows still processed.
`captureFrameToPng(WindowHandle, path)` selects a target; the path-only overload captures the first
remaining rendered window. `RenderSystem::setCamera(Camera)` changes the default view; a target's optional
`Camera` overrides it through `setCamera(WindowHandle, Camera)`. `clearCamera(WindowHandle)` removes that
override, so later default changes apply again. The engine lends its window owner to RenderSystem to
validate assignments before lazy Vulkan initialization; pending overrides move into targets when they
are created. Unknown, closed and opted-out windows return `Error::invalid_handle`. Camera uniforms and
directional cascades use the selected view and that target's drawable aspect ratio. Window views and
frame snapshots contain no ECS camera binding. The selected camera's clip planes drive projection and directional cascades,
with default near 0.1 and far 100. RenderScene stores only default-camera presence and imported-camera
owner tags for counting and per-instance removal; imported descriptors remain in the asset catalog.

Asset nodes retain mesh handles without a separate material list; `nodeMaterials()` resolves each mesh's material in the same
order. Mesh vertex/index counts come from their arrays. `sceneNodes()` reads `Graph::nodes()` in
creation order (pre-order for imported scenes), and node membership uses `tree.contains()`.
Graph uses a handle set for membership plus an order vector because sorted handles need not follow
creation order; labels live only on asset nodes. `AssetMesh::bounds` and public `meshBounds()` remain.
Catalog `field()` and `sceneField()` return const pointers, so counts and root queries borrow their source
without copying a handle vector or scene tree. Owning asset queries copy only their final result.
Simple-engine attribute `mesh*View()` accessors project const references from the shared interleaved
vertices; indices remain a const span. Views must not survive catalog mutation or destruction.
`ImportedAssetReadAccess::mesh_geometry` shares the vertex buffer with RenderScene; indices copy once. Import moves material and
mesh descriptors into their tables after saving their handles. Facade query signatures stay unchanged.
CPU `RenderMesh` stores no bounding box; `VulkanMesh::localBox` is the renderer's culling cache.

Asset materials retain typed `MaterialTextureSource` values. `AssetSystem::sceneTextureCount`
derives the number of distinct source paths across a scene's materials, counting shared file paths
and embedded image keys once. Render materials store texture-table indices, using `kNoTexture`
for absent maps. `RenderTexture::canonical_path` and the texture lookup index retain source identity
together with colour space for reuse and removal. Acquisition checks the supplied path first and
canonicalizes only misses. Decode failures remain cached by that key until `clearScene()`, so a
repaired file is retried only after clearing. The decoded greyscale flag avoids rescanning cached
HEIGHT sources. `simple::RenderSystem::textureDecodeCount()` counts all decode attempts over its
lifetime, including failures and re-decodes for upload; clearing resources does not reset this diagnostic.
`RenderTexture` retains its canonical file path or shared embedded source. After each window's render
call, `RenderScene::releaseUploadedPixels` swaps away pixel storage only when the renderer reports the
same resident generation through `uploadedTextureGeneration`. Pending or failed uploads keep their
pixels. `simple::RenderSystem::sceneTexturePixelBytes()` reports the remaining decoded CPU bytes.
Resident frames and swapchain recreation do not need these pixels. A later upload, including renderer
reinitialization, restores empty buffers through the scene's decoder and failure cache before recording
GPU copies. A missing source returns `Error::io_error` through `std::expected`; the Vulkan boundary
retains this decode error through cleanup. HEIGHT cache hits use only the stored greyscale flag.
`bindRenderScene` borrows mutable storage solely for texture pixel restoration and decode bookkeeping
(decoded extent/greyscale, attempt count and failure cache). Pixel release belongs to `RenderSystem`.

RenderScene indexes mesh, material and instance handles with unordered maps. Stable vector erasure
and purge rebuild the affected indices, while clear empties them. Purge collects mesh and material
references in one pass over instances and removes each unused range once; its return value remains
the number of removed meshes plus materials. Imported hierarchy traversal builds a node set once
and tracks visited handles in another set; asset scene membership uses `tree.contains()`.

Shared ownership covers imported vertex buffers and `MaterialTextureSource::embedded` / `RenderTexture::embedded`
(`shared_ptr<const EmbeddedImage>`), so the bytes of a texture embedded in a model file are not
copied when the asset catalog hands its texture sources out. `simple::Engine` owns the subsystems,
`RenderSystem` owns the `ForwardRenderer`, the renderer owns every Vulkan object through
`VulkanOwnedHandle` / `VulkanImage` / `VulkanBuffer` (VMA) and `cleanup()` runs in reverse creation
order. Replaced scene resources and shadow arrays move into a FIFO retirement queue tagged with
the last successful graphics submission serial. All targets use the same graphics queue, so a
completed frame fence covers that submission and every earlier submission, including other windows.
Pending descriptors keep their old contents until their own slot is safe to update; a skipped window
adopts current resources before its next submission. Static meshes keep one shared vertex/index pair;
the first vertex edit retires the static vertex buffer and creates per-window, per-slot vertex copies.
Static vertices and indices require device-local memory. One `submitOnce` copies both from upload
staging buffers and makes transfer writes visible to vertex/index reads; staging survives until the
copy fence completes. `BufferMemory` and the pure `allocationInfoFor` helper select VMA policies:
device-local storage has no host-access flags, upload has SEQUENTIAL_WRITE and MAPPED, and readback
has RANDOM and MAPPED. Both mapped policies require HOST_VISIBLE and HOST_COHERENT. Uniforms,
per-slot materials and dynamic vertices, and staging use upload; `VulkanReadback` uses readback.
`VulkanMemoryPolicyTests` checks the flags without a GPU, and `SimpleForwardRendererTests` checks
actual mesh and uniform allocation properties.
Material images store their mip count and expose every level through their views. Texture uploads
generate a complete chain with per-level linear blits and layout barriers when optimal-tiling format
features include linear filtering, BLIT_SRC and BLIT_DST; otherwise they use one level. A single
renderer-owned material sampler supplies linear minification, magnification and mip interpolation,
REPEAT addressing and VK_LOD_CLAMP_NONE. Device creation enables optional samplerAnisotropy only
when supported, and the sampler uses the device's maximum supported anisotropy. Cleanup destroys
the sampler after device idle, independently of texture retirement.
Each scene sync records all new texture images into one VulkanUploadBatch, which retains the staging
allocations and command buffer until one fence wait completes. Only successful completion commits
the new images and generations; releases alone need no upload submission. Existing per-slot
descriptor updates and serial retirement remain unchanged. textureUploadSubmitCount() counts
completed batches, including the first white fallback. VulkanTextureTests verifies filtered mip
pixels for rectangular sRGB and linear textures, capability fallbacks and sampler cleanup;
RenderTextureDedupTests requires exactly one submission for two new textures.
Device-idle waits are centralized and counted by `deviceWaitIdleCount()` for resize, teardown and
debug capture; ordinary scene sync and shadow growth use none. One-time upload fences still wait.
`RenderRuntimeUpdateTests` verifies unchanged idle counts, isolated vertex/material slots, texture
replacement, and retirement while two windows advance independently, with validation through cleanup. The
facade `Engine` is non-copyable and non-movable, so the wrapper references into its owned implementation stay
valid; `simple::RenderSystem` is non-copyable and non-movable too, because the renderer keeps
pointers into its `RenderScene`.

Command-buffer recording and submission borrow handles from the frame slot's RAII owners.
Descriptor sets retain the device and allocated handles; the target owns their pool. Instance creation
selects validation layers locally. GUI wiring uses preparation and recording callbacks, and SDL event
lookup uses the window system's id-to-index map.

Errors are `std::expected<T, Error>` up to the facade and `VkResult` below `RenderSystem`.
The error enum and `errorName()` contain only errors produced by the engine: unresolved texture paths
return `io_error`, failed Assimp imports return `asset_import_failed`, and repeated window initialization
returns `already_initialized`. The
engine does not throw (Vulkan-Hpp is built with `VULKAN_HPP_NO_EXCEPTIONS`); the vvppl
post-processing library does, and the renderer catches those exceptions and turns them into a
`VkResult`. The one deliberate break in propagation: a failing frame in `drawFrame` is logged
(capped at 16 messages) and skipped rather than propagated, so `run()` keeps going. A failed
swapchain rebuild keeps the device and is retried on the next frame.

## 4. Where the remaining weight is

**Three object models.** The same cube exists as an asset (`AssetMesh` in the catalog), as a
`RenderMesh` / `RenderInstance` in `RenderScene` (`RenderResources.ixx`, 590 lines), and as a
`VulkanMesh` on the GPU. AssetMesh builds one `shared_ptr<const std::vector<RenderVertex>>` at import,
and RenderMesh shares that same buffer. Object edits copy shared vertices before writing, preserving
the catalog and other instances; later edits reuse the private allocation. CPU vertices stay available
for edits and renderer reinitialization. The renderer reads the `RenderScene` meshes, materials, instances and
texture table directly (`bindRenderScene`); its only scene mutations restore texture pixels and update
decode bookkeeping. It keeps no additional CPU geometry copy beyond `RenderScene`. Lights are stored
only in the renderer's `Scene` (`ForwardDirectionalLight`, `ForwardPointLight`, `ForwardSpotLight`),
which shadow preparation, uniforms, light counts and presence queries read. `RenderScene` holds no lights.
`RenderSystem::clearLights()` clears these lists and their owner tags, preserving objects, resources,
cameras and scene instances; later instance removal still removes only that instance's remaining data.
The `testscene` light toggles clear and re-add only enabled lights, so disabled spots record no shadow passes.
`RenderSystem` tracks resource ownership (`render_objects_`, `object_sources_`,
`scene_instances_`, the imported mesh/material caches, the `owner` tag on imported lights and
cameras). The ECS sits in the world but nothing in rendering reads it; cameras and transforms flow
through `RenderSystem` calls, not through entities. Whether the ECS drives rendering or leaves the world is a teaching
decision, but one of the two should happen.

**Facade duplication.** Public wrapper methods forward to the selected implementation from their
class definitions in `src/RenderSystem.ixx`, `Assets.ixx`, `Gui.ixx` and `Window.ixx`; these facades
need no separate `.cpp` implementation units. The vocabulary in `src/Types.ixx` is shared by both
layers, not duplicated. The forwarding provides a common public API; it is fine as long as a second implementation is planned,
otherwise it is the largest pure overhead left.

**Shared containers.** `Vector.ixx` defines `vve::Vector<T>` as an alias of `std::vector<T>`;
sequences are contiguous and GPU uploads copy through `data()`. Growth follows standard vector
invalidation rules. Mesh editing copies the source before appending a clone, then looks up the
new mesh again; its instance belongs to a separate container. `Graph.ixx` is a tree used only for
the asset node hierarchy.

**Interface-heavy modules.** Most bodies still live inline in `.ixx` interface units (simple
`Window.ixx` 550, `Assets.ixx` 700, `RenderResources.ixx` 590, all Vulkan wrappers), so editing
one recompiles everything downstream. The `RenderSystem` / `ForwardRenderer` split into `.cpp`
implementation units is the model to extend if incremental build time starts to matter.

**Fixed choices worth knowing.** The view uses the camera position and forward direction
(the up vector comes from `detail::stableUp`), while projection uses its vertical FOV and clip planes.
Camera keeps no separate view matrix. Limits are 64 textures
(`VVE_MAX_SCENE_TEXTURES`; a full table returns `Error::capacity_exceeded`), 10 lights of each
kind, 4 cascades, 1024x1024 shadow maps. These limits, shadow matrix ranges, point-face count, shadow
near plane, occluded factor and receiver biases are defined in `shaders/simple_shared.h` for C++ and Slang.
Every window renders when `RendererId` is `"forward"` or
empty; `"none"` opts out. Other ids make `Engine::init()` return `Error::invalid_argument`.

## 5. Build notes

- `vve_test_support` builds `tests/support/TestSupport.ixx` as the C++23 module `VVE.TestSupport`.
  Tests import it and link the target for shared hidden-engine setup, text-fixture writing and RGBA
  capture measurements. It imports `std` and the simple engine without extending the public facade;
  tests keep their scene data, pixel masks, tolerances and assertions.
- The simple renderer requires Vulkan 1.3 devices with `VK_KHR_swapchain`, `dynamicRendering`,
  `shaderSampledImageArrayDynamicIndexing` and graphics/compute/presentation queues. Device selection
  evaluates the API version and feature contract through the pure `evaluateDeviceRequirements` helper;
  Vulkan 1.2 plus `VK_KHR_dynamic_rendering` is insufficient for its core rendering commands.
  If no device qualifies, one diagnostic names the rejected devices and versions. ImGui uses API
  version 1.3; the instance continues to request 1.4. The CPU-only `VulkanDeviceRequirementsTests`
  checks synthetic capabilities without creating Vulkan objects.
- C++23 modules with `import std` (clang >= 22 with libc++ on macOS/Linux, MSVC or clang on
  Windows; CMake >= 3.31, Ninja); vcpkg manifest.
- Clang builds use `-fno-aligned-allocation` (root `CMakeLists.txt`): clang 22 + libc++ `import std`
  does not reliably merge the implicitly declared aligned `operator new(size_t, align_val_t)` with the
  one the std module exports, which made `std::vector` growth "ambiguous" or crashed codegen depending
  on the import graph. Nothing in the engine is over-aligned, so the flag has no runtime effect.
- The white-box tests (`SimpleForwardRendererTests`, `GuiSystemTests` and the GPU tests
  `RenderFrameCountTests`, `MultiWindowRenderTests`, `RenderTextureDedupTests`, `RenderMaterialImportTests`, `RenderLightingCaptureTests`,
  `RenderSponzaCaptureTests`, `RenderMeshDedupTests`) link `ViennaVulkanEngine` and import the
  implementation modules from it; the `simple_modules` file set is PUBLIC for that reason. The ones
  that render pass `vve::simple::Windows` with hidden windows (`WindowDesc{.visible = false}`).
- Shaders are compiled at build time by `slangc -source-embed-style u32` into three generated headers
  (vertex, fragment, shadow vertex). Only `RendererResources.cpp` includes their arrays, and
  `VulkanShaderModule::create` borrows the words through `std::span<const std::uint32_t>`.
  Both `simple_forward.slang` and `simple_shared.h` trigger regeneration and an engine rebuild;
  the binary needs no runtime shader files or shader reflection.
