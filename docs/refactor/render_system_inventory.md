# Simple RenderSystem Inventory

Discovery scope: `src/versions/simple` only. Guidance re-read before inventory: `AGENTS.md` and `src/versions/simple/AGENTS.md`. The simple-engine guidance says the simple engine should stay explicit and minimal, with no virtual layer and no render/task graphs; this file records what exists today without proposing a replacement.

## RenderSystem Files

- `src/versions/simple/RenderSystem.ixx` lines 5-12: exports `VEEngine.Simple:RenderSystem` and imports `RenderPass`, `Window`, `Vulkan`, `Mesh`, `Scene`, and `Renderer`.
- `src/versions/simple/RenderSystem.ixx` lines 200-241: defines `RenderScene`, the CPU-side scene registry used by `RenderSystem`.
- `src/versions/simple/RenderSystem.ixx` lines 243-250: defines `RendererDescriptor`, including selected `RendererId`, `shadow_maps`, and declared pass span.
- `src/versions/simple/RenderSystem.ixx` lines 252-362: declares `RenderSystem`; lines 354-361 show it owns `RenderScene scene_`, concrete `Renderer renderer_`, render resource/function registries, frame counters, clear color, and initialization state.
- `src/versions/simple/Renderer.ixx` lines 20-56: defines the concrete Vulkan forward `Renderer` owned by `RenderSystem`; it owns Vulkan instance/device/swapchain/render pass/framebuffers/pipelines/command buffers/uniforms/descriptor sets/meshes and a CPU `Scene`.
- `src/versions/simple/RenderPass.ixx` lines 15-51: defines pass milestones, `RenderPassContract`, and `RenderGraph`.
- `src/versions/simple/GUI.ixx` lines 28 and 36-57: declares GUI pass contracts that are merged with renderer pass contracts.

## Public RenderSystem Method Categories

| Method | Lines | Category | Notes |
|---|---:|---|---|
| `createRenderer(RendererId)` | decl 255, def 545-548 | coordinator-level | Renderer selection by id; accepts `forward`, `stub`, or empty. |
| `createForwardRenderer()` | decl 256, def 537-542 | coordinator-level | Creates the current forward/stub renderer descriptor; descriptor includes pass span. |
| `buildRenderGraph(const RendererDescriptor &)` | decl 257, def 551-553 | renderer-specific | Public pass/graph registration helper; forwards renderer-declared passes into graph build. |
| `buildRenderGraph(std::span<const RenderPassContract>)` | decl 258, def 556-559 | renderer-specific | Public pass/graph registration helper for one pass list. |
| `buildRenderGraph(std::span<const std::span<const RenderPassContract>>)` | decl 259-260, def 561-575 | renderer-specific | Merges several pass lists by adding all pass nodes, then dependencies. Used for renderer plus GUI. |
| `createResource(RenderResourceKind, ObjectName)` | decl 261-262, def 604-612 | renderer-specific | Generic render resource registry, not user scene intent. |
| `createFunction(ObjectName, Vector<RenderResourceHandle>, Vector<RenderResourceHandle>)` | decl 263-264, def 614-629 | renderer-specific | Generic render function dependency registry. |
| `resourceCount()` | decl 265, def 631 | renderer-specific | Registry diagnostic for generic render resources. |
| `functionCount()` | decl 266, def 632 | renderer-specific | Registry diagnostic for generic render functions. |
| `resourceName(RenderResourceHandle)` | decl 267, def 646-649 | renderer-specific | Generic render resource registry lookup. |
| `resourceKind(RenderResourceHandle)` | decl 268, def 651-655 | renderer-specific | Generic render resource registry lookup. |
| `functionName(RenderFunctionHandle)` | decl 269, def 657-660 | renderer-specific | Generic render function registry lookup. |
| `addPlane(Vec2, LinearColor, Transform)` | decl 270-271, def 692-699 | coordinator-level | Public scene submission intent; implementation also mirrors into concrete backend via private `appendBackendObject` lines 662-689. |
| `addCuboid(Vec3, Vec3, LinearColor, Transform)` | decl 272-273, def 701-709 | coordinator-level | Public scene submission intent; implementation also mirrors into concrete backend. |
| `addTexturedCuboid(Vec3, Vec3, path, Transform)` | decl 274-276, def 711-726 | coordinator-level | Public scene submission intent with texture source validation; implementation also mirrors into concrete backend. |
| `clearScene()` | decl 277, def 728-732 | coordinator-level | Clears CPU scene and concrete renderer scene data. |
| `setCamera(Camera, PixelExtent)` | decl 278, def 733-738 | coordinator-level | Public camera submission; also calls concrete renderer camera setter. |
| `setDirectionalLight(Direction, LinearColor, LightIntensity, LinearColor)` | decl 279-280, def 739-748 | coordinator-level | Public light submission; also writes concrete renderer scene light. |
| `setPointLight(Position, LinearColor, LightIntensity, LightRange)` | decl 281, def 749-759 | coordinator-level | Public light submission; also writes concrete renderer scene light. |
| `setPointLight(Position, LinearColor, LightIntensity, LightRange, LinearColor)` | decl 282-283, def 760-769 | coordinator-level | Public light submission with ambient; also writes concrete renderer scene light. |
| `setSpotLight(Position, Direction, LinearColor, LightIntensity, LightRange, SpotConeAngle)` | decl 284-285, def 770-782 | coordinator-level | Public light submission; also writes concrete renderer scene light. |
| `setSpotLight(Position, Direction, LinearColor, LightIntensity, LightRange, SpotConeAngle, LinearColor)` | decl 286-287, def 783-796 | coordinator-level | Public light submission with ambient; also writes concrete renderer scene light. |
| `loadScene(Scene)` | decl 288, def 798-800 | coordinator-level | Scene submission to concrete renderer `Scene`. |
| `initialize(SDL_Window *)` | decl 289, def 802-809 | coordinator-level | Renderer lifetime setup; calls concrete `Renderer::init`. |
| `shutdown()` | decl 290, def 811-817 | coordinator-level | Renderer lifetime teardown; waits idle and calls concrete cleanup. |
| `setCamera(Vec3, Vec3)` | decl 291, def 819-821 | renderer-specific | Exposes concrete renderer camera setter rather than facade `Camera` intent. |
| `drawFrame(VulkanReadback *)` | decl 292, def 823-827 | renderer-specific | Concrete Vulkan frame draw/readback entry point. |
| `scene()` / `scene() const` | decl 293-294, def 829-835 | renderer-specific | Exposes concrete renderer `Scene` directly. |
| `backend()` / `backend() const` | decl 295-296, def 837-843 | renderer-specific | Exposes concrete `Renderer` directly. |
| `initialized()` | decl 297, def 845-847 | coordinator-level | Renderer lifetime state. |
| `sceneMeshCount()` | decl 298, def 849 | coordinator-level | CPU scene submission diagnostic. |
| `sceneMaterialCount()` | decl 299, def 850 | coordinator-level | CPU scene submission diagnostic. |
| `sceneInstanceCount()` | decl 300, def 851 | coordinator-level | CPU scene submission diagnostic. |
| `sceneVertexCount()` | decl 301, def 852 | coordinator-level | CPU scene submission diagnostic. |
| `sceneIndexCount()` | decl 302, def 853 | coordinator-level | CPU scene submission diagnostic. |
| `hasSceneCamera()` | decl 303, def 854 | coordinator-level | CPU scene/camera submission diagnostic. |
| `hasSceneDirectionalLight()` | decl 304, def 855 | coordinator-level | CPU scene/light submission diagnostic. |
| `hasScenePointLight()` | decl 305, def 856 | coordinator-level | CPU scene/light submission diagnostic. |
| `hasSceneSpotLight()` | decl 306, def 857 | coordinator-level | CPU scene/light submission diagnostic. |
| `captureFrameToPng(path)` | decl 307, def 859-891 | coordinator-level | Public frame capture intent; implementation currently performs Vulkan readback and PNG writing in `RenderSystem`. |
| `renderFrame(const WindowFrameData &)` | decl 308, def 893-904 | coordinator-level | Frame orchestration over window snapshot and render counter. |
| `renderFrame(WindowSystem &)` | decl 309, def 906-909 | coordinator-level | Frame orchestration using current window-system snapshot. |
| `renderedFrameCount()` | decl 310, def 911 | coordinator-level | Frame orchestration counter. |
| `presentedFrameCount()` | decl 311, def 912 | renderer-specific | Renderer presentation metric stub. |
| `triangleDrawCount()` | decl 312, def 913 | renderer-specific | Concrete draw diagnostic stub. |
| `triangleVertexCount()` | decl 313, def 914 | renderer-specific | Concrete draw diagnostic stub. |
| `sceneUploadCount()` | decl 314, def 915 | renderer-specific | Renderer upload diagnostic stub. |
| `sceneMeshDrawCount()` | decl 315, def 916 | renderer-specific | Concrete scene draw diagnostic stub. |
| `sceneInstanceDrawCount()` | decl 316, def 917 | renderer-specific | Concrete scene draw diagnostic stub. |
| `sceneDrawVertexCount()` | decl 317, def 918 | renderer-specific | Concrete scene draw diagnostic stub. |
| `sceneDrawIndexCount()` | decl 318, def 919 | renderer-specific | Concrete scene draw diagnostic stub. |
| `sceneDebugSampleCount()` | decl 319, def 920 | renderer-specific | Renderer debug sampling diagnostic stub. |
| `sceneCpuDebugSample(std::size_t)` | decl 320, def 921 | renderer-specific | Renderer debug sampling diagnostic stub. |
| `sceneGpuDebugSample(std::size_t)` | decl 321, def 922 | renderer-specific | Renderer debug sampling diagnostic stub. |
| `sceneDebugClipError(std::size_t)` | decl 322, def 923 | renderer-specific | Renderer debug sampling diagnostic stub. |
| `sceneDebugDepthError(std::size_t)` | decl 323, def 924 | renderer-specific | Renderer debug sampling diagnostic stub. |
| `sceneDebugLightSpaceError(std::size_t)` | decl 324, def 925 | renderer-specific | Renderer debug/light diagnostic stub. |
| `sceneDebugSpotLightSpaceError(std::size_t)` | decl 325, def 926 | renderer-specific | Renderer debug/light diagnostic stub. |
| `sceneDebugPointLightSpaceError(std::size_t)` | decl 326, def 927 | renderer-specific | Renderer debug/light diagnostic stub. |
| `sceneDebugLightingError(std::size_t)` | decl 327, def 928 | renderer-specific | Renderer debug/lighting diagnostic stub. |
| `sceneDebugShadowSampleError(std::size_t)` | decl 328, def 929 | renderer-specific | Renderer shadow/debug diagnostic stub. |
| `sceneDebugSpotShadowSampleError(std::size_t)` | decl 329, def 930 | renderer-specific | Renderer shadow/debug diagnostic stub. |
| `sceneDebugPointShadowSampleError(std::size_t)` | decl 330, def 931 | renderer-specific | Renderer shadow/debug diagnostic stub. |
| `sceneShadowDepthSampleCount()` | decl 331, def 932 | renderer-specific | Renderer shadow-depth diagnostic stub. |
| `sceneShadowDepthSample(std::size_t)` | decl 332, def 933 | renderer-specific | Renderer shadow-depth diagnostic stub. |
| `sceneShadowDepthError(std::size_t)` | decl 333, def 934 | renderer-specific | Renderer shadow-depth diagnostic stub. |
| `sceneSpotShadowDepthSampleCount()` | decl 334, def 935 | renderer-specific | Renderer spot-shadow diagnostic stub. |
| `sceneSpotShadowDepthSample(std::size_t)` | decl 335, def 936 | renderer-specific | Renderer spot-shadow diagnostic stub. |
| `sceneSpotShadowDepthError(std::size_t)` | decl 336, def 937 | renderer-specific | Renderer spot-shadow diagnostic stub. |
| `scenePointShadowDepthSampleCount()` | decl 337, def 938 | renderer-specific | Renderer point-shadow diagnostic stub. |
| `scenePointShadowDepthSample(std::size_t)` | decl 338, def 939 | renderer-specific | Renderer point-shadow diagnostic stub. |
| `scenePointShadowDepthError(std::size_t)` | decl 339, def 940 | renderer-specific | Renderer point-shadow diagnostic stub. |
| `lastRenderedWindowCount()` | decl 340, def 941 | coordinator-level | Frame orchestration/window snapshot diagnostic. |
| `preparedGpuTargetCount()` | decl 341, def 942 | renderer-specific | GPU target diagnostic stub. |
| `lastClearColor()` | decl 342, def 943 | renderer-specific | Renderer clear-color diagnostic stub. |

## Render Pass and Dependency Locations

- Milestone names are declared in `src/versions/simple/RenderPass.ixx` lines 15-37. The list includes `frame_begin`, `depth_prepass`, `shadow_depth`, `raytraced_shadow`, `gbuffer`, `deferred_lighting`, `raytraced_scene`, `scene_color`, `gui`, and `frame_finished`.
- Pass contract fields are declared in `src/versions/simple/RenderPass.ixx` lines 39-49: `name`, `depends_on`, shader entries, human-readable `inputs`/`outputs`, and `milestone`.
- The simple renderer/stub pass list is declared in `src/versions/simple/RenderSystem.ixx` lines 368-386. It contains `frame_begin`, `forward.color_pass`, `scene_color`, and `frame_finished`; dependencies are declared at lines 369-371.
- `createForwardRenderer()` returns that pass list in `src/versions/simple/RenderSystem.ixx` lines 537-542.
- GUI pass dependencies are declared in `src/versions/simple/GUI.ixx` lines 38-42 and the GUI pass contracts are declared at lines 42-57.
- Passes are merged into a `RenderGraph` by `RenderSystem::buildRenderGraph(std::span<const std::span<const RenderPassContract>>)` in `src/versions/simple/RenderSystem.ixx` lines 561-575. It adds nodes for every pass list first, then adds dependencies for every pass list.
- Node insertion is implemented by private `RenderSystem::addPass` in `src/versions/simple/RenderSystem.ixx` lines 577-586; dependency edges are implemented by private `RenderSystem::addDependencies` lines 588-602.
- The simple global render process is built in `Engine::buildDefaultGraphs()` in `src/versions/simple/Engine.ixx` lines 247-251 by calling `createForwardRenderer()`, combining `renderer.passes` with `gui_.passes()`, and assigning the built graph to `render_graph_`.
- Per-window debug graph dumping repeats the merge in `Engine::writeDebugGraphs()` in `src/versions/simple/Engine.ixx` lines 281-292: it reads each window renderer id, calls `createRenderer(renderer_id)`, combines renderer passes with GUI passes, builds a graph, and writes JSON.
- Concrete Vulkan render-pass objects and command-buffer pass execution are not declared through `RenderPassContract`; they live in `src/versions/simple/Renderer.ixx`: forward render pass ownership at lines 35 and 117-120, shadow pipeline/pass creation at lines 108-137, and command-buffer shadow/forward pass execution at lines 440-538.

## Existing RenderSystem-Related Tests

- `tests/RenderSystemTests.cpp` is a RenderSystem-focused test file, but it is not currently listed in `tests/CMakeLists.txt` lines 8-15. Its imports select `VEEngine.V5` when `VVE_ENGINE_IMPLEMENTATION_IS_V5` is defined and otherwise `VEEngine.V4` at lines 3-9, so it is related by subject but not a simple-engine test as written. It covers renderer descriptor/pass graph behavior at lines 11-172 and `RenderScene`/stateful `RenderSystem` behavior at lines 174-250.
- `tests/WorldTests.cpp` is included by `tests/CMakeLists.txt` line 13 and exercises simple/facade render graph dumps: it selects renderer id `forward` at lines 33-46 and checks dump output for `forward.color_pass`, `gui.overlay_pass`, and per-window renderer names at lines 49-64.
- `tests/UserSystemTests.cpp` is included by `tests/CMakeLists.txt` line 14 and exercises facade access to `vve::RenderSystem`: scene clearing/submission/camera/light setup at lines 18-35, scene state assertions at lines 79-87, debug graph dumping at lines 88-94, and frame counters at lines 97-106.
- `tests/CMakeLists.txt` lines 8-15 currently registers `HandleTests`, `SegmentedVectorTests`, `ECSTests`, `WindowInputTests`, `WindowOwnershipTests`, `WorldTests`, `UserSystemTests`, and `SceneSystemTests`; it does not register `RenderSystemTests.cpp`, `RenderGraphTests.cpp`, or other render-specific files.
