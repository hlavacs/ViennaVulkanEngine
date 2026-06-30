# Simple Renderer Interface Design

Scope: `src/versions/simple` only. This design uses `docs/refactor/render_system_inventory.md` as input and does not apply to `src/versions/v3`, `src/versions/v4`, or `src/versions/v5`.

## Goal

`RenderSystem` should become a renderer-agnostic coordinator. It should own renderer selection, renderer lifetime, scene submission, frame orchestration, frame capture intent, and global pass merging. Concrete drawing, Vulkan resources, lighting implementations, shadows, debug sampling, and renderer-local GPU state belong in renderer implementations such as `ForwardRenderer`, `DeferredRenderer`, or a future ray-tracing renderer.

The current inventory shows that `RenderSystem` owns both coordinator state and a concrete Vulkan `Renderer`: `src/versions/simple/RenderSystem.ixx` lines 252-362 declare `RenderSystem`, and lines 354-361 show `scene_`, concrete `Renderer renderer_`, resource/function registries, counters, clear color, and initialization state. The concrete Vulkan forward renderer is described in `src/versions/simple/Renderer.ixx` lines 20-56, where it owns Vulkan instance/device/swapchain/render pass/framebuffers/pipelines/command buffers/uniforms/descriptors/meshes and a CPU `Scene`. Those renderer-specific members should move behind the small renderer interface below.

## C++ Mechanism

Use a non-virtual, explicit C++23 mechanism:

```cpp
using SelectedRenderer = std::variant<ForwardRenderer, StubRenderer>;
```

Each renderer type exposes the same small member set. `RenderSystem` stores one `SelectedRenderer` and drives it with `std::visit`. A `RendererImplementation` concept may be added in the renderer implementation file or test file to document the required members and catch missing methods at compile time, but it should not become a new runtime layer.

Do not use an abstract base class, inheritance hierarchy, or virtual vtable. The simple engine guidance explicitly says "No virtual layer, keep things explicit for the time being" in `src/versions/simple/AGENTS.md`. A `std::variant` keeps selection explicit, uses the STL, has no vtable, keeps ownership local, and lets future renderers add renderer-private data without exposing it through `RenderSystem`.

Keep new types minimal:

- `ForwardRenderer`: rename or split from the existing concrete `Renderer` in `src/versions/simple/Renderer.ixx` lines 20-56.
- `StubRenderer`: optional only if the current `RendererId::stub` path still needs a no-op renderer.
- `SelectedRenderer`: a local alias, not a class.
- `RendererDiagnostics`: optional function-member view, only if renderer-specific diagnostics must remain reachable through tests.

## Common Renderer Members

The common renderer surface is a compile-time shape implemented by each concrete renderer type.

### Setup And Lifetime

```cpp
RendererId id() const;
void init(SDL_Window *window);
void shutdown();
bool initialized() const;
```

`RenderSystem::initialize(SDL_Window *)` and `RenderSystem::shutdown()` stay coordinator methods and call these members on the selected renderer. The renderer owns concrete Vulkan/SDL/GPU resources and cleanup. `RenderSystem` owns only the selected renderer object and its own coordinator counters.

### Scene Upload

```cpp
void uploadScene(const RenderScene &scene);
void clearScene();
void setCamera(Camera camera, PixelExtent extent);
void setDirectionalLight(Direction direction, LinearColor color, LightIntensity intensity, LinearColor ambient);
void setPointLight(Position position, LinearColor color, LightIntensity intensity, LightRange range, LinearColor ambient);
void setSpotLight(Position position, Direction direction, LinearColor color, LightIntensity intensity, LightRange range, SpotConeAngle cone, LinearColor ambient);
void loadScene(Scene scene);
```

The public user intent remains on `RenderSystem`: `addPlane`, `addCuboid`, `addTexturedCuboid`, `clearScene`, camera setters, light setters, and `loadScene`. `RenderSystem` updates its CPU `RenderScene` and then calls the selected renderer to upload or mirror the scene. The private backend mirror currently noted as `appendBackendObject` in `src/versions/simple/RenderSystem.ixx` lines 662-689 should move into the selected renderer.

### Frame Render

```cpp
void renderFrame(const WindowFrameData &frame, VulkanReadback *readback);
bool captureFrameToPng(const std::filesystem::path &path);
std::uint64_t presentedFrameCount() const;
```

`RenderSystem::renderFrame(const WindowFrameData &)` and `RenderSystem::renderFrame(WindowSystem &)` remain coordinator methods. They snapshot window data, update coordinator counters, and call the selected renderer. Vulkan readback and PNG writing should move out of `RenderSystem::captureFrameToPng(path)`, whose current implementation is at `src/versions/simple/RenderSystem.ixx` lines 859-891.

### Diagnostics

Generic coordinator diagnostics stay on `RenderSystem`: scene counts, camera/light submission flags, `initialized`, `renderedFrameCount`, and `lastRenderedWindowCount`.

Renderer-specific diagnostics are optional. If tests still need them, expose them as a non-owning function-member view returned by the selected renderer:

```cpp
struct RendererDiagnostics {
  std::uint64_t (*triangleDrawCount)(const void *self);
  std::uint64_t (*triangleVertexCount)(const void *self);
  std::uint64_t (*sceneUploadCount)(const void *self);
  std::uint64_t (*sceneMeshDrawCount)(const void *self);
  std::uint64_t (*sceneInstanceDrawCount)(const void *self);
  std::uint64_t (*sceneDrawVertexCount)(const void *self);
  std::uint64_t (*sceneDrawIndexCount)(const void *self);
  std::size_t (*sceneDebugSampleCount)(const void *self);
  /* Same pattern for CPU/GPU debug samples, clip/depth/light errors, and shadow-depth samples. */
};

std::optional<RendererDiagnostics> diagnostics() const;
```

This keeps shadow/debug/sample APIs out of the renderer-agnostic facade. The current shadow/debug/sample stub family is listed in `src/versions/simple/RenderSystem.ixx` declaration lines 319-339 and definition lines 920-940; it should move behind this optional diagnostics view or into renderer-specific tests. Renderers that do not support these diagnostics return `std::nullopt`.

### Render Pass And Dependency Declaration

```cpp
std::span<const RenderPassContract> passes() const;
```

Each renderer declares its own render passes and dependencies. `RenderSystem` builds the simple global render process by merging `selected_renderer.passes()` with GUI pass contracts and engine milestones. The existing pass data already uses `RenderPassContract` from `src/versions/simple/RenderPass.ixx` lines 39-49, with milestone names in lines 15-37. This is not a render/task graph in the engine-design sense; it is the existing simple pass contract list merged into the existing `RenderGraph`.

Current pass locations to preserve while moving ownership:

- The simple renderer/stub pass list is in `src/versions/simple/RenderSystem.ixx` lines 368-386.
- `createForwardRenderer()` returns that list in `src/versions/simple/RenderSystem.ixx` lines 537-542.
- GUI pass contracts are declared in `src/versions/simple/GUI.ixx` lines 38-57.
- `RenderSystem::buildRenderGraph(std::span<const std::span<const RenderPassContract>>)` merges pass lists in `src/versions/simple/RenderSystem.ixx` lines 561-575.
- `Engine::buildDefaultGraphs()` merges renderer and GUI passes in `src/versions/simple/Engine.ixx` lines 247-251.
- `Engine::writeDebugGraphs()` repeats the merge per window in `src/versions/simple/Engine.ixx` lines 281-292.
- Concrete Vulkan pass objects and command execution stay renderer-private; they currently live in `src/versions/simple/Renderer.ixx` lines 35, 108-137, 117-120, and 440-538.

## How RenderSystem Owns And Drives A Renderer

1. `RenderSystem::createRenderer(RendererId)` selects a concrete renderer by assigning `SelectedRenderer{ForwardRenderer{}}` or `SelectedRenderer{StubRenderer{}}`.
2. `RenderSystem::initialize(SDL_Window *)` visits the selected renderer and calls `init(window)`.
3. Scene-intent methods update `RenderSystem::scene_`, then call `uploadScene(scene_)` or the narrower renderer upload member when a smaller update is enough.
4. `RenderSystem::renderFrame(...)` remains responsible for window snapshot orchestration and `renderedFrameCount_`; it calls `renderer.renderFrame(frame, nullptr)`.
5. `RenderSystem::captureFrameToPng(path)` remains the user-intent API but calls `renderer.captureFrameToPng(path)`.
6. Graph construction uses `renderer.passes()` plus `gui.passes()`, then the existing `addPass` and `addDependencies` helpers from `src/versions/simple/RenderSystem.ixx` lines 577-602.
7. Renderer-specific tests may obtain optional diagnostics; generic `RenderSystem` tests should assert only coordinator behavior.

## Public Method Mapping

| Current public method | Inventory lines | Destination | Reason |
|---|---:|---|---|
| `createRenderer(RendererId)` | decl 255, def 545-548 | coordinator facade | User selects renderer intent; `RenderSystem` owns selection. |
| `createForwardRenderer()` | decl 256, def 537-542 | coordinator facade, then remove after callers use `createRenderer(...).passes()` | Transitional renderer selection helper; pass ownership moves to renderer. |
| `buildRenderGraph(const RendererDescriptor &)` | decl 257, def 551-553 | coordinator facade/internal helper | Graph merging is coordinator work; renderer only declares `passes()`. |
| `buildRenderGraph(std::span<const RenderPassContract>)` | decl 258, def 556-559 | coordinator facade/internal helper | Convenience for one pass list; no concrete drawing logic. |
| `buildRenderGraph(std::span<const std::span<const RenderPassContract>>)` | decl 259-260, def 561-575 | coordinator facade/internal helper | Merges renderer, GUI, and engine milestone contracts. |
| `createResource(RenderResourceKind, ObjectName)` | decl 261-262, def 604-612 | renderer interface | Generic render resource registry is renderer-local, not user intent. |
| `createFunction(ObjectName, Vector<RenderResourceHandle>, Vector<RenderResourceHandle>)` | decl 263-264, def 614-629 | renderer interface | Generic render function dependencies are renderer-local. |
| `resourceCount()` | decl 265, def 631 | optional diagnostics interface | Registry count is diagnostic, not facade intent. |
| `functionCount()` | decl 266, def 632 | optional diagnostics interface | Registry count is diagnostic, not facade intent. |
| `resourceName(RenderResourceHandle)` | decl 267, def 646-649 | optional diagnostics interface | Renderer resource lookup should not expose backend internals. |
| `resourceKind(RenderResourceHandle)` | decl 268, def 651-655 | optional diagnostics interface | Renderer resource lookup should not expose backend internals. |
| `functionName(RenderFunctionHandle)` | decl 269, def 657-660 | optional diagnostics interface | Renderer function lookup should not expose backend internals. |
| `addPlane(Vec2, LinearColor, Transform)` | decl 270-271, def 692-699 | coordinator facade | Public scene submission intent; renderer receives upload. |
| `addCuboid(Vec3, Vec3, LinearColor, Transform)` | decl 272-273, def 701-709 | coordinator facade | Public scene submission intent; renderer receives upload. |
| `addTexturedCuboid(Vec3, Vec3, path, Transform)` | decl 274-276, def 711-726 | coordinator facade | Public scene submission intent and validation; renderer receives upload. |
| `clearScene()` | decl 277, def 728-732 | coordinator facade | Clears CPU scene, then calls renderer `clearScene()`. |
| `setCamera(Camera, PixelExtent)` | decl 278, def 733-738 | coordinator facade | Public camera intent; renderer receives typed camera data. |
| `setDirectionalLight(Direction, LinearColor, LightIntensity, LinearColor)` | decl 279-280, def 739-748 | coordinator facade | Public light intent; renderer handles implementation. |
| `setPointLight(Position, LinearColor, LightIntensity, LightRange)` | decl 281, def 749-759 | coordinator facade | Public light intent; coordinator supplies default ambient. |
| `setPointLight(Position, LinearColor, LightIntensity, LightRange, LinearColor)` | decl 282-283, def 760-769 | coordinator facade | Public light intent; renderer handles implementation. |
| `setSpotLight(Position, Direction, LinearColor, LightIntensity, LightRange, SpotConeAngle)` | decl 284-285, def 770-782 | coordinator facade | Public light intent; coordinator supplies default ambient. |
| `setSpotLight(Position, Direction, LinearColor, LightIntensity, LightRange, SpotConeAngle, LinearColor)` | decl 286-287, def 783-796 | coordinator facade | Public light intent; renderer handles implementation. |
| `loadScene(Scene)` | decl 288, def 798-800 | coordinator facade | Public scene submission intent; renderer receives loaded scene. |
| `initialize(SDL_Window *)` | decl 289, def 802-809 | coordinator facade | Lifetime orchestration; delegates concrete init. |
| `shutdown()` | decl 290, def 811-817 | coordinator facade | Lifetime orchestration; delegates concrete shutdown. |
| `setCamera(Vec3, Vec3)` | decl 291, def 819-821 | renderer interface | Backend convenience setter; replace facade exposure with typed `Camera`. |
| `drawFrame(VulkanReadback *)` | decl 292, def 823-827 | renderer interface | Concrete render/readback entry point. |
| `scene()` / `scene() const` | decl 293-294, def 829-835 | renderer interface | Direct concrete `Scene` exposure should become renderer-private. |
| `backend()` / `backend() const` | decl 295-296, def 837-843 | renderer interface | Direct concrete renderer exposure should be removed from facade. |
| `initialized()` | decl 297, def 845-847 | coordinator facade | Generic lifetime state remains visible. |
| `sceneMeshCount()` | decl 298, def 849 | coordinator facade | CPU scene submission diagnostic. |
| `sceneMaterialCount()` | decl 299, def 850 | coordinator facade | CPU scene submission diagnostic. |
| `sceneInstanceCount()` | decl 300, def 851 | coordinator facade | CPU scene submission diagnostic. |
| `sceneVertexCount()` | decl 301, def 852 | coordinator facade | CPU scene submission diagnostic. |
| `sceneIndexCount()` | decl 302, def 853 | coordinator facade | CPU scene submission diagnostic. |
| `hasSceneCamera()` | decl 303, def 854 | coordinator facade | CPU camera submission diagnostic. |
| `hasSceneDirectionalLight()` | decl 304, def 855 | coordinator facade | CPU light submission diagnostic. |
| `hasScenePointLight()` | decl 305, def 856 | coordinator facade | CPU light submission diagnostic. |
| `hasSceneSpotLight()` | decl 306, def 857 | coordinator facade | CPU light submission diagnostic. |
| `captureFrameToPng(path)` | decl 307, def 859-891 | coordinator facade | User capture intent remains; renderer performs readback/PNG work. |
| `renderFrame(const WindowFrameData &)` | decl 308, def 893-904 | coordinator facade | Frame orchestration remains; renderer performs drawing. |
| `renderFrame(WindowSystem &)` | decl 309, def 906-909 | coordinator facade | Window-system snapshot orchestration remains. |
| `renderedFrameCount()` | decl 310, def 911 | coordinator facade | Coordinator frame counter. |
| `presentedFrameCount()` | decl 311, def 912 | renderer interface | Presentation metric belongs to renderer. |
| `triangleDrawCount()` | decl 312, def 913 | optional diagnostics interface | Concrete draw diagnostic. |
| `triangleVertexCount()` | decl 313, def 914 | optional diagnostics interface | Concrete draw diagnostic. |
| `sceneUploadCount()` | decl 314, def 915 | optional diagnostics interface | Renderer upload diagnostic. |
| `sceneMeshDrawCount()` | decl 315, def 916 | optional diagnostics interface | Concrete scene draw diagnostic. |
| `sceneInstanceDrawCount()` | decl 316, def 917 | optional diagnostics interface | Concrete scene draw diagnostic. |
| `sceneDrawVertexCount()` | decl 317, def 918 | optional diagnostics interface | Concrete scene draw diagnostic. |
| `sceneDrawIndexCount()` | decl 318, def 919 | optional diagnostics interface | Concrete scene draw diagnostic. |
| `sceneDebugSampleCount()` | decl 319, def 920 | optional diagnostics interface | Debug/sample stub family leaves renderer-agnostic facade. |
| `sceneCpuDebugSample(std::size_t)` | decl 320, def 921 | optional diagnostics interface | Debug/sample stub family leaves renderer-agnostic facade. |
| `sceneGpuDebugSample(std::size_t)` | decl 321, def 922 | optional diagnostics interface | Debug/sample stub family leaves renderer-agnostic facade. |
| `sceneDebugClipError(std::size_t)` | decl 322, def 923 | optional diagnostics interface | Debug/sample stub family leaves renderer-agnostic facade. |
| `sceneDebugDepthError(std::size_t)` | decl 323, def 924 | optional diagnostics interface | Debug/sample stub family leaves renderer-agnostic facade. |
| `sceneDebugLightSpaceError(std::size_t)` | decl 324, def 925 | optional diagnostics interface | Debug/light stub family leaves renderer-agnostic facade. |
| `sceneDebugSpotLightSpaceError(std::size_t)` | decl 325, def 926 | optional diagnostics interface | Debug/light stub family leaves renderer-agnostic facade. |
| `sceneDebugPointLightSpaceError(std::size_t)` | decl 326, def 927 | optional diagnostics interface | Debug/light stub family leaves renderer-agnostic facade. |
| `sceneDebugLightingError(std::size_t)` | decl 327, def 928 | optional diagnostics interface | Debug/lighting stub family leaves renderer-agnostic facade. |
| `sceneDebugShadowSampleError(std::size_t)` | decl 328, def 929 | optional diagnostics interface | Shadow/debug stub family leaves renderer-agnostic facade. |
| `sceneDebugSpotShadowSampleError(std::size_t)` | decl 329, def 930 | optional diagnostics interface | Shadow/debug stub family leaves renderer-agnostic facade. |
| `sceneDebugPointShadowSampleError(std::size_t)` | decl 330, def 931 | optional diagnostics interface | Shadow/debug stub family leaves renderer-agnostic facade. |
| `sceneShadowDepthSampleCount()` | decl 331, def 932 | optional diagnostics interface | Shadow-depth stub family leaves renderer-agnostic facade. |
| `sceneShadowDepthSample(std::size_t)` | decl 332, def 933 | optional diagnostics interface | Shadow-depth stub family leaves renderer-agnostic facade. |
| `sceneShadowDepthError(std::size_t)` | decl 333, def 934 | optional diagnostics interface | Shadow-depth stub family leaves renderer-agnostic facade. |
| `sceneSpotShadowDepthSampleCount()` | decl 334, def 935 | optional diagnostics interface | Spot-shadow stub family leaves renderer-agnostic facade. |
| `sceneSpotShadowDepthSample(std::size_t)` | decl 335, def 936 | optional diagnostics interface | Spot-shadow stub family leaves renderer-agnostic facade. |
| `sceneSpotShadowDepthError(std::size_t)` | decl 336, def 937 | optional diagnostics interface | Spot-shadow stub family leaves renderer-agnostic facade. |
| `scenePointShadowDepthSampleCount()` | decl 337, def 938 | optional diagnostics interface | Point-shadow stub family leaves renderer-agnostic facade. |
| `scenePointShadowDepthSample(std::size_t)` | decl 338, def 939 | optional diagnostics interface | Point-shadow stub family leaves renderer-agnostic facade. |
| `scenePointShadowDepthError(std::size_t)` | decl 339, def 940 | optional diagnostics interface | Point-shadow stub family leaves renderer-agnostic facade. |
| `lastRenderedWindowCount()` | decl 340, def 941 | coordinator facade | Window snapshot/frame orchestration diagnostic. |
| `preparedGpuTargetCount()` | decl 341, def 942 | optional diagnostics interface | GPU target diagnostic stub belongs to renderer diagnostics. |
| `lastClearColor()` | decl 342, def 943 | optional diagnostics interface | Renderer clear-color diagnostic stub belongs to renderer diagnostics. |

## Follow-Up Code Tasklets

1. In `src/versions/simple/Renderer.ixx`, rename or split the concrete Vulkan `Renderer` into `ForwardRenderer`, preserving concrete Vulkan ownership and behavior in that file.
2. In `src/versions/simple/Renderer.ixx`, add the common non-virtual member set: lifetime, scene upload, frame render, pass declaration, and optional diagnostics access.
3. In `src/versions/simple/RenderSystem.ixx`, replace concrete `Renderer renderer_` with `SelectedRenderer` and implement `createRenderer(RendererId)` using `std::variant` assignment.
4. In `src/versions/simple/RenderSystem.ixx`, move renderer pass contracts from lines 368-386 to the selected renderer and make graph construction consume `renderer.passes()`.
5. In `src/versions/simple/RenderSystem.ixx`, move backend scene mirroring from `appendBackendObject` lines 662-689 into renderer scene upload members.
6. In `src/versions/simple/RenderSystem.ixx`, move Vulkan readback/PNG work from `captureFrameToPng(path)` lines 859-891 into the renderer.
7. In `src/versions/simple/Engine.ixx`, update default graph and debug graph construction at lines 247-251 and 281-292 to obtain passes through the selected renderer path.
8. Split tests so `WorldTests.cpp` and `UserSystemTests.cpp` keep coordinator/facade assertions, while renderer-specific draw, shadow, debug, and sample assertions move into forward-renderer tests.
