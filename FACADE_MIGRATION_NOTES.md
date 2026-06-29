# Facade Migration Notes

Discovery date: 2026-06-29

## Scope

This report catalogs CMake-discovered example/application targets and their current use of the ViennaVulkanEngine facade versus concrete engine internals. No source, header, or build files were changed.

## Official Facade Surface

The engine does not currently expose a traditional public textual umbrella header for applications. The public application-facing surface is a C++23 module facade declared in the `ViennaVulkanEngine` target.

Primary facade import:

- `import VEEngine;` from `src/Engine.ixx`

Facade module files exposed by `src/CMakeLists.txt` through the `ViennaVulkanEngine` target's public C++ module file set:

- `src/Engine.ixx` exports module `VEEngine` and re-exports the facade pieces.
- `src/Error.ixx` exports module `VEEngine.Error`.
- `src/Math.ixx` exports module `VEEngine.Math`.
- `src/Handle.ixx` exports module `VEEngine.Handle`.
- `src/Vector.ixx` exports module `VEEngine.Vector`.
- `src/Types.ixx` exports module `VEEngine.Types`.
- `src/ECS.ixx` exports facade partition `VEEngine:ECS`.
- `src/Window.ixx` exports facade partition `VEEngine:Window`.
- `src/World.ixx` exports facade partition `VEEngine:World`.
- `src/Assets.ixx` exports facade partition `VEEngine:Assets`.
- `src/RenderSystem.ixx` exports facade partition `VEEngine:RenderSystem`.
- `src/Gui.ixx` exports facade partition `VEEngine:Gui`.

The current selected concrete implementation is `simple`, configured in `src/CMakeLists.txt` with `VVE_ENGINE_IMPLEMENTATION_NAMESPACE=simple`. Implementation modules live under `src/versions/simple` and export `VEEngine.Simple...` modules in namespace `vve::simple`; application targets should not import or name these directly.

## Example Targets

### `game`

Files:

- Target: `examples/game/CMakeLists.txt`
- Source: `examples/game/game.cpp`

Facade use:

- Does not import `VEEngine`.
- Does not use namespace `vve` facade symbols directly.

Direct implementation dependency:

- Links `ViennaVulkanEngineSimple` directly in `examples/game/CMakeLists.txt`.
- Adds dependency on `ViennaVulkanEngineSimple`.

Internal module imports:

- `import VEEngine.Simple.Math;`
- `import VEEngine.Simple.Mesh;`
- `import VEEngine.Simple;`
- `import VEEngine.Simple.Scene;`

Internal symbols used:

- `vve::simple::Scene`, `Object`, `RenderSystem`, `SpotLight`, `PointLight`, `DirectionalLight`.
- `vve::simple::Vec2`, `Vec3`.
- Mesh helpers: `makePlane`, `makeCube`.
- Math helpers: `identityMat4`, `translate`, `normalize`, `subtract`, `clamp`, `cross`, `add`, `scale`.
- Direct scene mutation through `renderSystem.scene().spotLight`, `pointLight`, and `directionalLight`.
- Renderer control through `renderSystem.loadScene(...)`, `initialize(...)`, `setCamera(...)`, `drawFrame()`, and `shutdown()`.

Other non-facade platform API use:

- Includes `<SDL3/SDL.h>`, `<SDL3/SDL_main.h>`, and `<vulkan/vulkan.h>`.
- Creates and manages an `SDL_Window` directly.

### `physics`

Files:

- Target: `examples/physics/CMakeLists.txt`
- Source: `examples/physics/physics.cpp`

Facade use:

- Does not import `VEEngine`.
- Does not use namespace `vve` facade symbols directly.

Direct implementation dependency:

- Links `ViennaVulkanEngineSimple` directly in `examples/physics/CMakeLists.txt`.
- Adds dependency on `ViennaVulkanEngineSimple`.

Internal module imports:

- `import VEEngine.Simple;`
- `import VEEngine.Simple.Scene;`

Internal symbols used:

- `vve::simple::RenderSystem`.
- `vve::simple::makeSampleScene()`.
- Renderer control through `renderSystem.loadScene(...)`, `initialize(...)`, `drawFrame()`, and `shutdown()`.

Other non-facade platform API use:

- Includes `<SDL3/SDL.h>`, `<SDL3/SDL_main.h>`, and `<vulkan/vulkan.h>`.
- Creates and manages an `SDL_Window` directly.

### `sponza`

Files:

- Target: `examples/sponza/CMakeLists.txt`
- Source: `examples/sponza/sponza.cpp`

Facade use:

- Does not import `VEEngine`.
- Does not use namespace `vve` facade symbols directly.

Direct implementation dependency:

- Links `ViennaVulkanEngineSimple` directly in `examples/sponza/CMakeLists.txt`.
- Adds dependency on `ViennaVulkanEngineSimple`.

Internal module imports:

- `import VEEngine.Simple;`
- `import VEEngine.Simple.Scene;`

Internal symbols used:

- `vve::simple::RenderSystem`.
- `vve::simple::makeSampleScene()`.
- Renderer control through `renderSystem.loadScene(...)`, `initialize(...)`, `drawFrame()`, and `shutdown()`.

Other non-facade platform API use:

- Includes `<SDL3/SDL.h>`, `<SDL3/SDL_main.h>`, and `<vulkan/vulkan.h>`.
- Creates and manages an `SDL_Window` directly.

### `light_shadow_debug`

Files:

- Target: `examples/light_shadow_debug/CMakeLists.txt`
- Source: `examples/light_shadow_debug/light_shadow_debug.cpp`

Facade use:

- Does not import `VEEngine`.
- Does not use namespace `vve` facade symbols directly.

Direct implementation dependency:

- Links `ViennaVulkanEngineSimple` directly in `examples/light_shadow_debug/CMakeLists.txt`.
- Adds dependency on `ViennaVulkanEngineSimple`.

Internal module imports:

- `import VEEngine.Simple.Math;`
- `import VEEngine.Simple.Mesh;`
- `import VEEngine.Simple;`
- `import VEEngine.Simple.Scene;`
- `import VEEngine.Simple.Vulkan;`

Internal symbols used:

- `vve::simple::Mesh`, `Scene`, `Object`, `PointLight`, `DirectionalLight`, `SpotLight`, `RenderSystem`, `VulkanReadback`.
- `vve::simple::Vec2`, `Vec3`.
- Mesh helpers: `makePlane`, `makeCube`.
- Math helpers: `identityMat4`, `translate`, `scale`.
- Readback helper: `vve::simple::writeReadbackPng(...)`.
- Direct renderer backend access through `renderSystem.backend()`.
- Direct backend field access: `renderer.physicalDevice.physicalDevice`, `renderer.device.device`, `renderer.device.graphicsQueue`, `renderer.commandPool.commandPool`, `renderer.swapchain.extent`, `renderer.swapchain.imageFormat`, and `renderer.lastReadbackCaptureResult`.
- Renderer control through `renderSystem.loadScene(...)`, `initialize(...)`, `drawFrame(...)`, and `shutdown()`.

Other non-facade platform API use:

- Includes `<SDL3/SDL.h>`, `<SDL3/SDL_main.h>`, and `<vulkan/vulkan.h>`.
- Creates and manages an `SDL_Window` directly.
- Uses raw Vulkan symbols and calls including `VkResult`, `VK_SUCCESS`, swapchain image format constants, and `vkDeviceWaitIdle(...)`.

### `simple_forward_demo`

Files:

- Target: `examples/simple_forward_demo/CMakeLists.txt`
- Source: `examples/simple_forward_demo/simple_forward_demo.cpp`

Facade use:

- Does not import `VEEngine`.
- Does not use namespace `vve` facade symbols directly.

Direct implementation dependency:

- Links `ViennaVulkanEngineSimple` directly in `examples/simple_forward_demo/CMakeLists.txt`.
- Adds dependency on `ViennaVulkanEngineSimple`.

Internal module imports:

- `import VEEngine.Simple;`
- `import VEEngine.Simple.Scene;`
- `import VEEngine.Simple.Vulkan;`

Internal symbols used:

- `vve::simple::RenderSystem`, `VulkanReadback`.
- `vve::simple::makeSampleScene()`.
- Readback helper: `vve::simple::writeReadbackPng(...)`.
- Direct renderer backend access through `renderSystem.backend()`.
- Direct backend field access: `renderer.physicalDevice.physicalDevice`, `renderer.device.device`, `renderer.device.graphicsQueue`, `renderer.commandPool.commandPool`, `renderer.swapchain.extent`, `renderer.swapchain.imageFormat`, and `renderer.lastReadbackCaptureResult`.
- Direct readback internals: `readback.buffer.buffer`, `readback.buffer.memory`, `readback.create(...)`, `readback.pixelBytes()`, and `readback.cleanup()`.
- Renderer control through `renderSystem.loadScene(...)`, `initialize(...)`, `drawFrame(...)`, and `shutdown()`.

Other non-facade platform API use:

- Includes `<SDL3/SDL.h>`, `<SDL3/SDL_main.h>`, and `<vulkan/vulkan.h>`.
- Creates and manages an `SDL_Window` directly.
- Uses raw Vulkan symbols and calls including `VkResult`, `VK_SUCCESS`, `VK_NULL_HANDLE`, swapchain image format constants, and `vkDeviceWaitIdle(...)`.

## Other CMake Application-Style Targets

The repository also defines CMake test executables through `tests/CMakeLists.txt`. These are not examples, but they are executable targets.

### `HandleTests`

- Source: `tests/HandleTests.cpp`
- Imports `VEEngine`.
- No direct implementation module import, direct implementation namespace use, or `ViennaVulkanEngineSimple` link was found.

### `SegmentedVectorTests`

- Source: `tests/SegmentedVectorTests.cpp`
- Imports `VEEngine`.
- No direct implementation module import, direct implementation namespace use, or `ViennaVulkanEngineSimple` link was found.

### `ECSTests`

- Source: `tests/ECSTests.cpp`
- Imports `VEEngine`.
- No direct implementation module import, direct implementation namespace use, or `ViennaVulkanEngineSimple` link was found.

### `WindowInputTests`

- Source: `tests/WindowInputTests.cpp`
- Imports `VEEngine`.
- No direct implementation module import, direct implementation namespace use, or `ViennaVulkanEngineSimple` link was found.

### `WindowOwnershipTests`

- Source: `tests/WindowOwnershipTests.cpp`
- Imports `VEEngine`.
- No direct implementation module import, direct implementation namespace use, or `ViennaVulkanEngineSimple` link was found.

### `WorldTests`

- Source: `tests/WorldTests.cpp`
- Imports `VEEngine`.
- No direct implementation module import, direct implementation namespace use, or `ViennaVulkanEngineSimple` link was found.

### `UserSystemTests`

- Source: `tests/UserSystemTests.cpp`
- Imports `VEEngine`.
- No direct implementation module import, direct implementation namespace use, or `ViennaVulkanEngineSimple` link was found.

### `SceneSystemTests`

- Source: `tests/SceneSystemTests.cpp`
- Imports `VEEngine`.
- No direct implementation module import, direct implementation namespace use, or `ViennaVulkanEngineSimple` link was found.

## Unbuilt Test Sources Observed

The following `tests/*.cpp` files exist in the repository but are not added by the current `tests/CMakeLists.txt`:

- `tests/ImplementationTests.cpp`: imports `VEEngine`; builds a path under `src/versions/<engine>/shaders` using the facade string `vve::engineImplementationNamespaceName`.
- `tests/RenderGraphTests.cpp`: imports `VEEngine.V4` or `VEEngine.V5` and aliases `vve::v4` or `vve::v5`.
- `tests/TaskGraphTests.cpp`: imports `VEEngine.V4` or `VEEngine.V5` and aliases `vve::v4` or `vve::v5`.
- `tests/RenderSystemTests.cpp`: imports `VEEngine.V4` or `VEEngine.V5` and aliases `vve::v4` or `vve::v5`.
- `tests/ResourceSystemTests.cpp`: imports `VEEngine.V4` or `VEEngine.V5` and aliases `vve::v4` or `vve::v5`.
- `tests/ShaderSystemTests.cpp`: imports `VEEngine.V4` or `VEEngine.V5`, aliases `vve::v4` or `vve::v5`, and reads shader files under `src/versions/<engine>/shaders`.
- `tests/ImplementationShaderSystemTests.cpp`: imports `VEEngine.V4` or `VEEngine.V5`, aliases `vve::v4` or `vve::v5`, and reads shader files under `src/versions/<engine>/shaders`.

## Summary

All five example targets currently depend on the `simple` implementation directly through both CMake and source-level module imports. None of the examples currently use the official `import VEEngine;` facade path. The CMake test targets listed in `tests/CMakeLists.txt` use only the facade import, while several unbuilt legacy test sources still reference retired `VEEngine.V4` or `VEEngine.V5` implementation modules.

## Facade API Surface (Current)

This section records the application-facing symbols currently exported by `import VEEngine;`, based on the facade files in `src/*.ixx`. The primary module `src/Engine.ixx` exports `VEEngine`, imports the selected implementation module, and re-exports `VEEngine.Error`, `VEEngine.Math`, `VEEngine.Handle`, `VEEngine.Vector`, `VEEngine.Types`, plus the `:ECS`, `:Window`, `:World`, `:Assets`, `:RenderSystem`, and `:Gui` partitions. The only intended application namespace is `vve`.

- `src/Engine.ixx`: `vve::engineImplementationNamespaceName`, `template<class...> vve::Engine`, `vve::MakeEngine`, and `vve::makeEngine`. `Engine` exposes constructors from `EngineConfig` or typed options, `versionMajor()`, `getVersionMajor()`, `versionName()`, `world()`, `init()`, `run()`, `step()`, and `writeDebugGraphs(...)`.
- `src/Error.ixx`: `vve::Error` and `vve::errorName(...)`.
- `src/Math.ixx`: in `vve::math`, aliases `Scalar`, `Vec2`, `Vec3`, `Vec4`, `Quat`, `Mat4`, and functions `add`, `clamp`, `cross`, `dot`, `identityMat4`, `identityQuat`, `inverse`, `length`, `lengthSquared`, `lookAt`, `max`, `min`, `multiply`, `normalize`, `one`, `oneVec3`, `perspective`, `perspectiveVulkan`, `scale`, `subtract`, `translate`, `zero`, and `zeroVec3`. Namespace `vve` also re-exports the math types plus `identityMat4`, `identityQuat`, `one`, `oneVec3`, `zero`, and `zeroVec3`.
- `src/Handle.ixx`: `template<class TTag> vve::TypedHandle`, `vve::makeCounterHandle<THandle>()`, `vve::makeHandleForTest<THandle>(...)`, and `vve::makeSlotMapHandleForTest<THandle>(...)`.
- `src/Vector.ixx`: `template<class T> vve::Vector`, `template<class T> vve::VectorConstRange`, and `vve::makeRange(...)`.
- `src/Types.ixx`: handle tags and aliases `Entity`, `SceneHandle`, `WindowHandle`, `NodeHandle`, `MeshHandle`, `MaterialHandle`, `TextureHandle`, `LightHandle`, and `CameraHandle`; strong data types `Position`, `Direction`, `Scale`, `Rotation`, `LinearColor`, `LightIntensity`, `LightRange`, `SpotConeAngle`, `FovY`, `ClipPlanes`, `DeltaTime`, `PixelExtent`, `ObjectName`, `RendererId`, `FrameCount`, `ApplicationName`, `MaxFrames`, `FrameContext`, `EngineConfig`, `VertexCount`, `IndexCount`, `TextureChannelCount`, `Transform`, `Bounds`, and `Camera`; enum `FrameStatus`. `Camera` also exposes static `Camera::lookAt(...)`.
- `src/ECS.ixx`: `vve::DefaultECSTraits`, `template<class TTraits> vve::BasicECS`, and `vve::ECS`. `BasicECS` exposes `create()`, `exists(...)`, `erase(...)`, component operations `add<T>(...)`, `get<T>(...)`, `tryGet<T>(...)`, `put<T>(...)`, `has<T>(...)`, `remove<T>(...)`, and `view<T...>()`.
- `src/Window.ixx`: `vve::WindowSetup` builder methods `id(...)`, `title(...)`, `extent(...)`, `position(...)`, `renderer(...)`, `resizable(...)`, and `visible(...)`; `vve::WindowSetups` with initializer-list construction and `add(...)`; `vve::InputState` methods for per-frame key and mouse state; `vve::Window` read-only accessors `handle()`, `id()`, `title()`, `extent()`, `rendererId()`, `camera()`, `focused()`, `minimized()`, and `shouldClose()`; `vve::WindowSystem` accessors and camera binding methods `name()`, `input()`, `windowCount()`, `windows()`, `findWindow(...)`, `setWindowCamera(...)`, `clearWindowCamera(...)`, `windowCamera(...)`, `setActiveCamera(...)`, and `activeCamera()`.
- `src/World.ixx`: `template<class...> vve::World` with `get<T>()`, `template<class...> vve::UserSystems`, `vve::MakeUserSystems`, and `vve::makeUserSystems`. `World` stores references to facade wrappers and user systems; it does not expose implementation descriptors.
- `src/Assets.ixx`: `vve::AssetSystem` with scene loading and query functions `addScene(...)`, `loadScene(path)`, `containsScene(...)`, `sceneName(...)`, scene count queries, `sceneRootNode(...)`, scene collection queries, node hierarchy queries, node property queries, mesh property and vertex/index queries, and material texture queries. It returns facade handles, facade data types, and `vve::Vector`.
- `src/RenderSystem.ixx`: debug data structs `vve::RenderDebugSample` and `vve::RenderShadowDepthSample`; wrapper `vve::RenderSystem`. The wrapper exposes scene authoring functions `clearScene()`, `setCamera(Camera, PixelExtent)`, `setDirectionalLight(...)`, `setPointLight(...)`, `setSpotLight(...)`, `addPlane(...)`, and `addCuboid(...)`; scene/render counters and booleans; CPU/GPU debug sample accessors; shadow-depth sample accessors; `lastRenderedWindowCount()`, `preparedGpuTargetCount()`, and `lastClearColor()`. It does not expose `loadScene(vve::simple::Scene)`, `initialize(SDL_Window*)`, `drawFrame(...)`, `shutdown()`, `scene()`, `backend()`, or any simple renderer internals.
- `src/Gui.ixx`: `vve::GuiSystem`, currently only a facade wrapper type with construction from the selected implementation.

## Migration Gap: physics

The simplest example is `examples/physics/physics.cpp`. Its implementation-specific imports are `import VEEngine.Simple;` and `import VEEngine.Simple.Scene;`. It uses these `vve::simple` symbols:

- `vve::simple::RenderSystem`: no default-constructible facade equivalent exists. The facade equivalent type is `vve::RenderSystem`, but applications obtain it through `auto world = engine.world(); auto &render = world.get<vve::RenderSystem>();` after constructing a `vve::Engine`. This is sufficient for facade-owned rendering, but not for the standalone `vve::simple::RenderSystem renderSystem{};` pattern.
- `vve::simple::makeSampleScene()`: no facade symbol currently returns or applies this sample scene. The facade intentionally does not expose `vve::simple::Scene`. The current facade can approximate the sample through `vve::RenderSystem::clearScene()`, `addCuboid(...)`, `setDirectionalLight(...)`, and `setSpotLight(...)`, but there is no one-call facade equivalent to the concrete helper.
- `vve::simple::RenderSystem::loadScene(vve::simple::Scene)`: no facade equivalent exists, and exposing this exact signature would leak an internal descriptor. The facade-safe replacement is to author the scene through `vve::RenderSystem` methods or add a facade-level sample-scene helper that consumes only facade types.
- `vve::simple::RenderSystem::initialize(SDL_Window*)`: no facade equivalent with a raw `SDL_Window *` exists. The existing facade lifecycle is `vve::Engine::init()`, with windows described by `vve::WindowSetups` and `vve::WindowSetup`; this is the preferred migration path because applications should not own SDL windows directly.
- `vve::simple::RenderSystem::drawFrame()`: no public facade render-system method has this name. The facade equivalent is `vve::Engine::step()`, which advances the implementation engine and invokes the implementation render system internally.
- `vve::simple::RenderSystem::shutdown()`: no explicit facade shutdown function exists. The facade path relies on facade/implementation object lifetime for cleanup; if an explicit shutdown hook is required for examples, it must be added to `vve::Engine` rather than exposing renderer internals.

Concrete facade additions required to migrate `examples/physics/physics.cpp` to use only `import VEEngine;`:

1. `vve::RenderSystem::loadSampleScene()` in `src/RenderSystem.ixx`, or an equivalently named facade helper in the same partition, to populate the current three-cube sample using only facade data types. This replaces `vve::simple::makeSampleScene()` plus `vve::simple::RenderSystem::loadScene(...)` without exposing `vve::simple::Scene`.
2. Optional only if deterministic early teardown is required by the example contract: `vve::Engine::shutdown()` in `src/Engine.ixx`. Current facade migration can otherwise use `vve::Engine::init()` and repeated `vve::Engine::step()` with RAII cleanup, so no render-system `initialize(SDL_Window*)`, `drawFrame()`, or `shutdown()` facade additions should be added for the physics example.

## Migration Gap: game

Source: `examples/game/game.cpp`.

### Internal modules

| Current use | Facade-only replacement |
| --- | --- |
| `import VEEngine.Simple.Math;` | `import VEEngine;`, then use `vve::math::*` and facade math/type aliases. |
| `import VEEngine.Simple.Mesh;` | `import VEEngine;`, then author supported geometry through `vve::RenderSystem::addPlane(...)` and `vve::RenderSystem::addCuboid(...)`. |
| `import VEEngine.Simple;` | `import VEEngine;`, then create the engine through `vve::makeEngine(...)`. |
| `import VEEngine.Simple.Scene;` | `import VEEngine;`, then author scene state through `auto render = engine.world().get<vve::RenderSystem>();`. |

### Type and helper mapping

| Current use in `game.cpp` | Facade-only replacement or gap |
| --- | --- |
| `vve::simple::RenderSystem renderSystem{}` | Use `auto engine = vve::makeEngine(vve::ApplicationName{"game"}, vve::WindowSetups{...}); auto render = engine.world().get<vve::RenderSystem>();`. The facade render system is obtained from `world().get<vve::RenderSystem>()`; it is not application-owned or default-constructible. |
| `vve::simple::Scene` | No facade scene descriptor should be exposed. Author the current scene with `vve::RenderSystem::clearScene()`, `addPlane(...)`, `addCuboid(...)`, `setPointLight(...)`, `setDirectionalLight(...)`, `setSpotLight(...)`, and `setCamera(...)`. |
| `vve::simple::Object` | Replace each object with one render authoring call: floor object -> `vve::RenderSystem::addPlane(vve::Vec2{6.0F, 4.0F}, vve::LinearColor{...})`; each cube object -> `vve::RenderSystem::addCuboid(vve::Vec3{-0.5F, -0.5F, -0.5F}, vve::Vec3{0.5F, 0.5F, 0.5F}, vve::LinearColor{...}, vve::Transform{.translation = vve::Position{.value = ...}})`. |
| `vve::simple::SpotLight` | Use `vve::RenderSystem::setSpotLight(vve::Position, vve::Direction, vve::LinearColor, vve::LightIntensity, vve::LightRange, vve::SpotConeAngle)`. GAP for exact parity: no facade getter for the current spot light, no ambient parameter, and no inner cone parameter. Proposed signature: `void vve::RenderSystem::setSpotLight(vve::Position position, vve::Direction direction, vve::LinearColor color, vve::LightIntensity intensity, vve::LightRange range, vve::SpotConeAngle inner_cone, vve::SpotConeAngle outer_cone, vve::LinearColor ambient);`. |
| `vve::simple::PointLight` | Use `vve::RenderSystem::setPointLight(vve::Position, vve::LinearColor, vve::LightIntensity, vve::LightRange)`. GAP for exact parity: no facade getter for the current point light and no ambient parameter, so `pointLight.ambient = 0.0F` cannot be represented. Proposed signature: `void vve::RenderSystem::setPointLight(vve::Position position, vve::LinearColor color, vve::LightIntensity intensity, vve::LightRange range, vve::LinearColor ambient);`. |
| `vve::simple::DirectionalLight` | Use `vve::RenderSystem::setDirectionalLight(vve::Direction, vve::LinearColor, vve::LightIntensity, vve::LinearColor)`. GAP for exact parity: no facade getter for the current directional light; the example must store its own default values unless a getter is added. Proposed signature: `std::optional<vve::DirectionalLightState> vve::RenderSystem::directionalLight() const;` with `DirectionalLightState` containing only facade types. |
| `vve::simple::Vec2` | Use `vve::Vec2` or `vve::math::Vec2`. |
| `vve::simple::Vec3` | Use `vve::Vec3` or `vve::math::Vec3`. |
| `vve::simple::makePlane(groundHalfExtent)` | Use `vve::RenderSystem::addPlane(vve::Vec2 half_extent, vve::LinearColor color, vve::Transform transform = {})`. |
| `vve::simple::makeCube()` | Use `vve::RenderSystem::addCuboid(vve::Vec3 minimum, vve::Vec3 maximum, vve::LinearColor color, vve::Transform transform = {})` for the unit cubes. GAP for arbitrary meshes beyond plane/cuboid: no facade method accepts application-supplied vertices/indices or mesh handles for custom geometry. Proposed signature: `std::expected<vve::MeshHandle, vve::Error> vve::RenderSystem::addMesh(std::span<const vve::Vec3> positions, std::span<const vve::Vec3> colors, std::span<const vve::Vec2> texcoords, std::span<const std::uint32_t> indices);`. |
| `vve::simple::identityMat4()` | Use `vve::identityMat4()` or `vve::math::identityMat4()`. For object placement prefer `vve::Transform{}`. |
| `vve::simple::translate(matrix, offset)` | Use `vve::math::translate(matrix, offset)`. For object placement prefer `vve::Transform{.translation = vve::Position{.value = offset}}`. |
| `vve::simple::normalize(value)` | Use `vve::math::normalize(value)`. |
| `vve::simple::subtract(left, right)` | Use `vve::math::subtract(left, right)`. |
| `vve::simple::clamp(value, minimum, maximum)` | Use `vve::math::clamp(value, minimum, maximum)`. |
| `vve::simple::cross(left, right)` | Use `vve::math::cross(left, right)`. |
| `vve::simple::add(left, right)` | Use `vve::math::add(left, right)`. |
| `vve::simple::scale(value, factor)` | Use `vve::math::scale(value, factor)`. |

### Scene and renderer access

| Current use in `game.cpp` | Facade-only replacement or gap |
| --- | --- |
| `renderSystem.loadScene(makeGameScene())` | Replace with `render.clearScene()`, then the individual facade authoring calls listed above. GAP for the crate texture and `useBaseColorTexture`: no current facade method binds a base-color texture or combines per-object material selection with transform. Proposed signature: `std::expected<void, vve::Error> vve::RenderSystem::addTexturedCuboid(vve::Vec3 minimum, vve::Vec3 maximum, std::filesystem::path base_color_texture, vve::Transform transform = {});`. |
| `renderSystem.scene().spotLight` read to save defaults | GAP: no facade light-state getter. For the current example, store the default facade values in application variables before calling `setSpotLight(...)`; for exact backend default readback add `std::optional<vve::SpotLightState> vve::RenderSystem::spotLight() const;` with facade-only fields. |
| `renderSystem.scene().pointLight` read to save defaults | GAP: no facade light-state getter. For the current example, store the default facade values in application variables before calling `setPointLight(...)`; for exact backend default readback add `std::optional<vve::PointLightState> vve::RenderSystem::pointLight() const;` with facade-only fields. |
| `renderSystem.scene().directionalLight` read to save defaults | GAP: no facade light-state getter. For the current example, store the default facade values in application variables before calling `setDirectionalLight(...)`; for exact backend default readback add `std::optional<vve::DirectionalLightState> vve::RenderSystem::directionalLight() const;` with facade-only fields. |
| `renderSystem.scene().spotLight = defaultSpotLight` | Use `vve::RenderSystem::setSpotLight(...)` with values stored by the application. Exact restore of backend defaults is blocked by the missing spot-light getter listed above. |
| `renderSystem.scene().spotLight.intensity.value = 0.0F` | Use `vve::RenderSystem::setSpotLight(...)` with `vve::LightIntensity{.value = 0.0F}`. |
| `renderSystem.scene().spotLight.ambient = 0.0F` | GAP: current `setSpotLight(...)` has no ambient parameter. Use the extended `setSpotLight(...)` signature proposed above. |
| `renderSystem.scene().spotLight.range.value = 0.0F` | Use `vve::RenderSystem::setSpotLight(...)` with `vve::LightRange{.value = 0.0F}`. |
| `renderSystem.scene().pointLight = defaultPointLight` | Use `vve::RenderSystem::setPointLight(...)` with values stored by the application. Exact restore of backend defaults is blocked by the missing point-light getter listed above. |
| `renderSystem.scene().pointLight.intensity = 0.0F` | Use `vve::RenderSystem::setPointLight(...)` with `vve::LightIntensity{.value = 0.0F}`. |
| `renderSystem.scene().pointLight.ambient = 0.0F` | GAP: current `setPointLight(...)` has no ambient parameter. Use the extended `setPointLight(...)` signature proposed above. |
| `renderSystem.scene().pointLight.range = 0.0F` | Use `vve::RenderSystem::setPointLight(...)` with `vve::LightRange{.value = 0.0F}`. |
| `renderSystem.scene().directionalLight = defaultDirectionalLight` | Use `vve::RenderSystem::setDirectionalLight(...)` with values stored by the application. Exact restore of backend defaults is blocked by the missing directional-light getter listed above. |
| `renderSystem.scene().directionalLight.intensity.value = 0.0F` | Use `vve::RenderSystem::setDirectionalLight(...)` with `vve::LightIntensity{.value = 0.0F}`. |
| `renderSystem.scene().directionalLight.ambient = 0.0F` | Use `vve::RenderSystem::setDirectionalLight(...)` with `vve::LinearColor{.value = vve::Vec3{0.0F, 0.0F, 0.0F}}`. |
| `renderSystem.initialize(window)` | Use `vve::Engine::init()` after creating the engine with `vve::WindowSetups{vve::WindowSetup{}.id("main").title("VVE Simple Game").extent(vve::PixelExtent{.width = 960, .height = 540}).renderer(vve::RendererId{.value = "forward"}).resizable(true)}`. |
| `renderSystem.setCamera(cameraEye, vve::simple::add(cameraEye, forward))` | Use `vve::RenderSystem::setCamera(vve::Camera::lookAt(vve::Position{.value = cameraEye}, vve::Position{.value = vve::math::add(cameraEye, forward)}, vve::Direction{.value = worldUp}), vve::PixelExtent{.width = 960, .height = 540})`. GAP for window-size-aware camera control: no direct facade helper binds the active window extent automatically. Proposed signature: `void vve::RenderSystem::setCamera(vve::Camera camera);` using the active render target extent. |
| `renderSystem.drawFrame()` | Use `vve::Engine::step()`, which advances platform events, user systems, and rendering. |
| `renderSystem.shutdown()` | No direct facade call required for the migrated example; use facade object lifetime after the frame loop. Optional explicit teardown would belong on `vve::Engine`, e.g. `std::expected<void, vve::Error> vve::Engine::shutdown();`. |

### Platform API mapping

| Current use in `game.cpp` | Facade-only replacement or gap |
| --- | --- |
| `#define SDL_MAIN_HANDLED` | Remove; facade application imports `VEEngine` and does not manage SDL main integration directly. |
| `#include <SDL3/SDL.h>` | Remove; window and input access should go through `vve::WindowSetups`, `vve::WindowSystem`, and `vve::InputState`. |
| `#include <SDL3/SDL_main.h>` | Remove; facade application imports `VEEngine` only. |
| `#include <vulkan/vulkan.h>` | Remove; no Vulkan symbol is used directly by the facade migration. |
| `SDL_SetMainReady()` | No facade replacement needed for application code; engine initialization owns platform setup through `vve::Engine::init()`. |
| `SDL_SetHint(SDL_HINT_VULKAN_LIBRARY, VVE_SDL_VULKAN_LIBRARY)` | No current facade replacement. GAP only if applications must choose the Vulkan loader path at runtime. Proposed signature: `vve::EngineConfig` or `vve::makeEngine(...)` option `vve::VulkanLibraryPath{std::filesystem::path value}`. |
| `SDL_InitSubSystem(SDL_INIT_VIDEO)` | Use `vve::Engine::init()`. |
| `SDL_CreateWindow("VVE Simple Game", 960, 540, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE)` | Use `vve::WindowSetups{vve::WindowSetup{}.id("main").title("VVE Simple Game").extent(vve::PixelExtent{.width = 960, .height = 540}).renderer(vve::RendererId{.value = "forward"}).resizable(true)}` passed to `vve::makeEngine(...)`. |
| `SDL_Window *window` ownership | Use facade-owned windows; read them through `auto windows = engine.world().get<vve::WindowSystem>(); windows.findWindow("main")` or `windows.windows()`. |
| `SDL_GetError()` | Use `vve::errorName(result.error())` for facade `std::expected<..., vve::Error>` failures. GAP for platform-specific diagnostic text: no facade method exposes backend platform error strings. Proposed signature: `std::string vve::Engine::lastPlatformError() const;`. |
| `SDL_QuitSubSystem(SDL_INIT_VIDEO)` | No direct facade call; facade engine lifetime owns platform teardown. |
| `SDL_DestroyWindow(window)` | No direct facade call; facade-owned windows are cleaned up by the engine implementation. |
| `SDL_Event`, `SDL_PollEvent(&event)`, `SDL_EVENT_QUIT` | Use `vve::Engine::step()` and stop when it returns `vve::FrameStatus::stopped`. |
| `SDL_EVENT_KEY_DOWN`, `SDLK_ESCAPE`, `SDLK_O`, `SDLK_P`, `SDLK_L`, `event.key.repeat` | Use `auto input = engine.world().get<vve::WindowSystem>().input(); input.wasKeyPressed(keycode)` after `engine.step()`. GAP: the facade accepts raw `std::int32_t` keycodes but does not define facade key constants, so a no-SDL application cannot name Escape/O/P/L portably. Proposed signature: `enum class vve::Key { escape, o, p, l, left, right, up, down, w, a, s, d };` plus `bool vve::InputState::wasKeyPressed(vve::Key key) const;`. |
| `SDL_GetKeyboardState(nullptr)`, `SDL_SCANCODE_LEFT`, `SDL_SCANCODE_RIGHT`, `SDL_SCANCODE_UP`, `SDL_SCANCODE_DOWN`, `SDL_SCANCODE_W`, `SDL_SCANCODE_A`, `SDL_SCANCODE_S`, `SDL_SCANCODE_D` | Use `vve::InputState::isKeyDown(...)` from `engine.world().get<vve::WindowSystem>().input()`. GAP: same missing facade key constants as above. |

### Required current-facade scene authoring sequence

1. Create the facade engine with `vve::makeEngine(vve::ApplicationName{"game"}, vve::WindowSetups{...})`.
2. Call `engine.init()`.
3. Get the render facade with `auto render = engine.world().get<vve::RenderSystem>();`.
4. Call `render.clearScene()`.
5. Add the ground with `render.addPlane(vve::Vec2{6.0F, 4.0F}, vve::LinearColor{.value = vve::Vec3{0.1F, 0.6F, 0.2F}})`.
6. Add the three unit cubes with `render.addCuboid(vve::Vec3{-0.5F, -0.5F, -0.5F}, vve::Vec3{0.5F, 0.5F, 0.5F}, vve::LinearColor{.value = vve::Vec3{0.55F, 0.55F, 0.55F}}, vve::Transform{.translation = vve::Position{.value = ...}})`.
7. Set lights with `render.setPointLight(...)`, `render.setDirectionalLight(...)`, and `render.setSpotLight(...)`; exact ambient/default-read parity needs the light gaps listed above.
8. Set the camera each frame with `render.setCamera(vve::Camera::lookAt(...), vve::PixelExtent{.width = 960, .height = 540})`; exact resize-aware camera parity needs the camera extent gap listed above.
9. Drive the loop with `engine.step()` and `vve::FrameStatus`.

### Explicit capabilities needed by `game` that no current facade method provides

| Capability needed | Proposed single facade-only method signature |
| --- | --- |
| Base-color texture binding for the crate cubes, replacing `Scene::baseColorTexture` plus `Object::useBaseColorTexture`. | `std::expected<void, vve::Error> vve::RenderSystem::addTexturedCuboid(vve::Vec3 minimum, vve::Vec3 maximum, std::filesystem::path base_color_texture, vve::Transform transform = {});` |
| General arbitrary mesh authoring beyond current plane/cuboid helpers. | `std::expected<vve::MeshHandle, vve::Error> vve::RenderSystem::addMesh(std::span<const vve::Vec3> positions, std::span<const vve::Vec3> colors, std::span<const vve::Vec2> texcoords, std::span<const std::uint32_t> indices);` |
| Reusing a mesh with per-object material/transform combinations without duplicating geometry. | `std::expected<void, vve::Error> vve::RenderSystem::addObject(vve::MeshHandle mesh, vve::MaterialHandle material, vve::Transform transform = {});` |
| Creating a material that references a base-color texture path. | `std::expected<vve::MaterialHandle, vve::Error> vve::RenderSystem::addMaterial(vve::LinearColor base_color, std::optional<std::filesystem::path> base_color_texture = std::nullopt);` |
| Reading current/default light state for toggle restore. | `std::optional<vve::PointLightState> vve::RenderSystem::pointLight() const;`, `std::optional<vve::DirectionalLightState> vve::RenderSystem::directionalLight() const;`, and `std::optional<vve::SpotLightState> vve::RenderSystem::spotLight() const;` with state structs composed only of facade types. |
| Point-light ambient control. | `void vve::RenderSystem::setPointLight(vve::Position position, vve::LinearColor color, vve::LightIntensity intensity, vve::LightRange range, vve::LinearColor ambient);` |
| Spot-light ambient and inner-cone control. | `void vve::RenderSystem::setSpotLight(vve::Position position, vve::Direction direction, vve::LinearColor color, vve::LightIntensity intensity, vve::LightRange range, vve::SpotConeAngle inner_cone, vve::SpotConeAngle outer_cone, vve::LinearColor ambient);` |
| Camera control that does not require the application to hard-code the render target extent every frame. | `void vve::RenderSystem::setCamera(vve::Camera camera);` |
| SDL-free key names for facade input. | `bool vve::InputState::isKeyDown(vve::Key key) const;`, `bool vve::InputState::wasKeyPressed(vve::Key key) const;`, and `bool vve::InputState::wasKeyReleased(vve::Key key) const;`. |

## Internals Lockdown Plan

### Current public-interface coupling

No public facade interface currently has an `export import VEEngine.Simple...` line; the implementation-module exposure comes from direct non-exported imports in the exported module interfaces and from re-exporting facade modules that themselves import simple partitions.

`src/Engine.ixx` is the umbrella `export module VEEngine;` interface and also declares the public `vve::Engine` wrapper. It imports the simple implementation directly: `src/Engine.ixx:12: import VEEngine.Simple;`. It names implementation types throughout the public module interface: `src/Engine.ixx:61: using Impl = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::Engine;`, `src/Engine.ixx:63: static auto implementationOption(WindowSetups option) -> VVE_ENGINE_IMPLEMENTATION_NAMESPACE::Windows;`, `src/Engine.ixx:65: static VVE_ENGINE_IMPLEMENTATION_NAMESPACE::UserSystemTasks`, `src/Engine.ixx:68: static VVE_ENGINE_IMPLEMENTATION_NAMESPACE::UserSystemTasks`, `src/Engine.ixx:84: const VVE_ENGINE_IMPLEMENTATION_NAMESPACE::WindowFrameData &window_frame);`, and `src/Engine.ixx:87: [[nodiscard]] static VVE_ENGINE_IMPLEMENTATION_NAMESPACE::UserSystemTasks`. It stores the engine implementation by value: `src/Engine.ixx:90: Impl impl_{};`. To remove the transitive simple requirement, `Engine` needs an opaque owning handle such as `std::unique_ptr<Impl>` or a non-template engine state Pimpl whose full implementation type is only imported in a module implementation unit. The inline/template definitions that construct `Windows`, `UserSystemTasks`, and `WindowFrameData` also need to stop naming simple types in the interface; those conversions should move behind facade-owned option/task/frame types and out-of-line implementation functions.

`src/RenderSystem.ixx` imports the simple implementation directly: `src/RenderSystem.ixx:3: import VEEngine.Simple;`. It names the implementation type in the wrapper interface: `src/RenderSystem.ixx:71: using Impl = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::RenderSystem;`. It does not store the implementation by value; it stores a reference: `src/RenderSystem.ixx:144: Impl &impl_; ///< Selected implementation render system.`. To remove the transitive simple requirement, the public class must not expose `Impl` in the constructor or member layout. Use an opaque pointer/reference-sized facade handle, move the constructor that accepts the real simple render system out of the public interface, and move every forwarding body that calls `impl_` into a module implementation unit importing `VEEngine.Simple`.

`src/Window.ixx` imports the simple implementation directly: `src/Window.ixx:3: import VEEngine.Simple;`. It names several implementation types: `src/Window.ixx:14: using Impl = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::WindowDesc;`, `src/Window.ixx:56: using Impl = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::Windows;`, `src/Window.ixx:90: using Impl = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::InputState;`, `src/Window.ixx:124: using Impl = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::Window;`, and `src/Window.ixx:148: using Impl = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::WindowSystem;`. It stores two implementation values by value: `src/Window.ixx:52: Impl impl_{};` and `src/Window.ixx:71: Impl impl_{};`. The runtime wrappers store references instead: `src/Window.ixx:120: Impl &impl_;`, `src/Window.ixx:144: Impl &impl_;`, and `src/Window.ixx:200: Impl &impl_;`. To remove the transitive simple requirement, `WindowSetup` and `WindowSetups` should become facade-owned value descriptors with no conversion operator to simple types in the interface; conversion to simple `WindowDesc`/`Windows` should happen inside `Engine` implementation code. `InputState`, `Window`, and `WindowSystem` should hold opaque handles and forward out-of-line from a module implementation unit.

`src/World.ixx` imports the simple implementation directly: `src/World.ixx:3: import VEEngine.Simple;`. The current `World` template does not store an implementation member by value and does not name a `VVE_ENGINE_IMPLEMENTATION_NAMESPACE` type in its interface; it stores facade/user references in `src/World.ixx:67: std::tuple<TObjects...> objects_; ///< Public wrappers and user-supplied system references.`. To remove the transitive simple requirement, the direct import can be deleted once the imported facade partitions still provide all names used by this file. No Pimpl is required for `World` itself.

`src/Assets.ixx` imports the simple implementation directly: `src/Assets.ixx:3: import VEEngine.Simple;`. It names the implementation type in the wrapper interface: `src/Assets.ixx:14: using Impl = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::AssetSystem;`. It does not store the implementation by value; it stores a reference: `src/Assets.ixx:131: Impl &impl_;`. To remove the transitive simple requirement, hide the constructor's concrete implementation parameter behind an opaque handle and move all forwarding bodies, including `Vector<T>::implementation_type` conversion details, into a module implementation unit that imports the simple modules.

`src/Gui.ixx` imports the simple implementation directly: `src/Gui.ixx:3: import VEEngine.Simple;`. It names the implementation type in the wrapper interface: `src/Gui.ixx:12: using Impl = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::GuiSystem;`. It does not store the implementation by value; it stores a reference: `src/Gui.ixx:22: Impl &impl_;`. To remove the transitive simple requirement, make `GuiSystem` an opaque non-owning wrapper and move the real simple-system constructor out of the exported interface.

`src/ECS.ixx` imports the simple implementation directly: `src/ECS.ixx:3: import VEEngine.Simple;`. It names implementation types in public aliases and wrapper declarations: `src/ECS.ixx:13: using DefaultECSTraits = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::DefaultECSTraits; ///< Facade ECS traits.`, `src/ECS.ixx:19: using Impl = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::BasicECS<TTraits>;`, and `src/ECS.ixx:74: using Impl = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::ECS;`. It does not store by value; `BasicECS` stores a reference: `src/ECS.ixx:68: Impl &impl_;`. This is harder than the other reference wrappers because `BasicECS` exposes templated component methods whose definitions call implementation templates inline. To remove the simple requirement, either make the ECS implementation itself facade-owned, or introduce a type-erased non-template runtime backend plus out-of-line non-template operations; the templated component API can remain in the facade only if it targets facade-owned storage or a backend interface that does not name simple types.

`src/Handle.ixx` imports a simple partition directly: `src/Handle.ixx:3: import VEEngine.Simple.Handle;`. It names the implementation handle type in the public template: `src/Handle.ixx:14: using implementation_type = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::TypedHandle<TTag>;`. It stores that implementation value by value: `src/Handle.ixx:49: implementation_type impl_{};`. To remove the transitive simple requirement, make `TypedHandle` a facade-owned strong type over `std::uint64_t` and move or duplicate only the small public handle packing helpers in the facade layer; simple internals should accept facade handles instead of requiring conversions to simple handle types.

`src/Error.ixx` imports a simple partition directly: `src/Error.ixx:3: import VEEngine.Simple.Error;`. It names implementation symbols in public aliases: `src/Error.ixx:11: using Error = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::Error; ///< Facade error vocabulary.` and `src/Error.ixx:12: using VVE_ENGINE_IMPLEMENTATION_NAMESPACE::errorName; ///< Facade diagnostic name lookup.`. It stores no implementation member. To remove the transitive simple requirement, define `vve::Error` and `vve::errorName` in the facade layer and have simple return that facade error type, or convert simple errors inside non-exported implementation units.

`src/Math.ixx` imports a simple partition directly: `src/Math.ixx:3: import VEEngine.Simple.Math;`. It names implementation math types and functions in public aliases/usings: `src/Math.ixx:11: using Scalar = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::math::Scalar; ///< Facade scalar type.` through `src/Math.ixx:16: using Mat4 = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::math::Mat4; ///< Facade 4x4 matrix.`, and `src/Math.ixx:18: using VVE_ENGINE_IMPLEMENTATION_NAMESPACE::math::add;` through `src/Math.ixx:40: using VVE_ENGINE_IMPLEMENTATION_NAMESPACE::math::zeroVec3;`. It stores no implementation member. To remove the transitive simple requirement, make math a facade-owned value/function module, with simple importing and using those definitions rather than the facade aliasing simple math.

`src/Vector.ixx` imports a simple partition directly: `src/Vector.ixx:3: import VEEngine.Simple.Vector;`. It names the implementation vector type in the public template: `src/Vector.ixx:13: using implementation_type = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::Vector<T>;`. It stores that implementation value by value: `src/Vector.ixx:86: implementation_type impl_{};`. To remove the transitive simple requirement, replace this with a facade-owned container, likely a thin `std::vector<T>` wrapper, and update simple-facing conversion points to copy or move between facade vectors and any simple internal containers in non-exported code.

`src/Types.ixx` has no direct `VEEngine.Simple` or `VVE_ENGINE_IMPLEMENTATION_NAMESPACE` import, but it re-exports facade modules that currently import simple internals: `src/Types.ixx:3: export import VEEngine.Error;`, `src/Types.ixx:4: export import VEEngine.Handle;`, `src/Types.ixx:5: export import VEEngine.Math;`, and `src/Types.ixx:6: export import VEEngine.Vector;`. It stores no implementation member. To remove the transitive simple requirement, first decouple `Error`, `Handle`, `Math`, and `Vector`; then `Types` becomes a pure facade data module.

### Why the current build leaks internals

The facade modules are public CMake module sources on `ViennaVulkanEngine`: `src/CMakeLists.txt:60: target_sources(ViennaVulkanEngine`, `src/CMakeLists.txt:61: PUBLIC`, and `src/CMakeLists.txt:62: FILE_SET CXX_MODULES FILES`. The simple implementation modules are also public module sources on the same target: `src/versions/simple/CMakeLists.txt:1: target_sources(ViennaVulkanEngine`, `src/versions/simple/CMakeLists.txt:4: PUBLIC`, and `src/versions/simple/CMakeLists.txt:5: FILE_SET CXX_MODULES FILES`.

Removing the old `ViennaVulkanEngineSimple` INTERFACE target alone therefore cannot prevent an application from writing `import VEEngine.Simple.*`: those modules are still exported as public C++ module file-set entries of `ViennaVulkanEngine`, the target applications already link. The leak persists after that target removal because the provider of the BMIs is now the main engine target's public module file set, not a separate link target.

Moving the simple modules to `PRIVATE` now would break the build for the CMake modules reason that public sources cannot require modules provided only by private sources. Every public facade interface listed above imports `VEEngine.Simple` or a `VEEngine.Simple.*` partition, so a consumer compiling `import VEEngine` needs the simple BMIs as part of the target's public module dependency graph. A separate static-library split also does not fix this while the public facade interfaces import implementation modules: if `ViennaVulkanEngine` keeps exporting facades that import simple modules, those imported modules must still be visible to consumers as public module providers, regardless of whether their object code lives in a shared library or a static library.

### Smallest-first follow-up tasklets

1. Delete the unused `import VEEngine.Simple;` from `src/World.ixx` only, then build. This is independently buildable because `World.ixx` currently names no simple implementation type.
2. Make `VEEngine.Error` facade-owned by defining `vve::Error` and `vve::errorName` without importing `VEEngine.Simple.Error`; update simple code to use or convert to the facade error type, then build.
3. Make `VEEngine.Math` facade-owned by moving the small scalar/vector/matrix aliases and functions out of the simple namespace dependency; update simple to import/use facade math or convert internally, then build.
4. Make `VEEngine.Handle` facade-owned over `std::uint64_t`; update simple handle users to accept facade handles or convert at non-exported boundaries, then build.
5. Make `VEEngine.Vector` facade-owned, preferably backed by `std::vector<T>`; update facade/simple conversion points such as `AssetSystem::facadeVector` so no public interface exposes `Vector<T>::implementation_type`, then build.
6. After steps 2-5, verify `src/Types.ixx` is pure facade data because its re-exported dependencies no longer import simple modules, then build.
7. Replace `WindowSetup` and `WindowSetups` by-value simple members with facade-owned descriptors; move conversion to simple `WindowDesc`/`Windows` into non-exported engine implementation code, then build.
8. Convert `GuiSystem`, `InputState`, `Window`, `WindowSystem`, `AssetSystem`, and `RenderSystem` from concrete `Impl &` public layouts to opaque non-owning handles with out-of-line forwarding functions in module implementation units importing simple modules, one wrapper module at a time, building after each wrapper.
9. Decouple `ECS.ixx` by choosing a facade-owned ECS store or a type-erased backend that keeps templated component operations from naming `VVE_ENGINE_IMPLEMENTATION_NAMESPACE::BasicECS` in the public interface, then build.
10. Decouple `Engine.ixx` last by replacing the by-value `Impl impl_{}` with an opaque owning implementation handle, moving `Windows`, `UserSystemTasks`, `WindowFrameData`, construction, stepping, and rendering glue behind non-exported implementation functions, then build.
11. Once no public `src/*.ixx` imports `VEEngine.Simple` or names `VVE_ENGINE_IMPLEMENTATION_NAMESPACE`, move `src/versions/simple/*.ixx` from the public `CXX_MODULES` file set to a private/non-exported module file set or private implementation target, then build applications and tests to verify `import VEEngine.Simple.*` is no longer provided to application targets.
