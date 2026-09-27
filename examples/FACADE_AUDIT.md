# Facade API Audit

Status: **PASS** — all examples use only the public facade.

## Public facade API

The official application-facing API is the facade module `VEEngine`, exported from `src/Engine.ixx`. User code should import `VEEngine` and use namespace `vve`, not implementation namespaces such as `vve::simple`.

Facade source files:

- `src/Engine.ixx`: exports module `VEEngine` and re-exports `VEEngine.Error`, `VEEngine.Math`, `VEEngine.Handle`, `VEEngine.Vector`, `VEEngine.Types` and all partitions below; defines `vve::Engine<TSystems...>`, `vve::EngineBuilder<TSystems...>`, `vve::WindowFrameInfo`, and `vve::WindowFrameData`.
- `src/implementations/simple.ixx`: exports partition `VEEngine:Implementation`; defines `vve::engineImplementationNamespaceName` and the `vve::detail::*Impl` aliases the wrappers use. It is the only facade file that names the implementation (`import VEEngine.Simple`); CMake compiles the one file selected by `VVE_ENGINE_IMPLEMENTATION_NAMESPACE`.
- `src/World.ixx`: exports partition `VEEngine:World`; defines `vve::World<TObjects...>`, `vve::UserSystems<TSystems...>`, and `vve::makeUserSystems`.
- `src/ECS.ixx`: exports partition `VEEngine:ECS`; re-exports `vve::DefaultECSTraits`, `vve::BasicECS<TTraits>`, and `vve::ECS` from module `VEEngine.ECSContainer` (`src/ECSContainer.ixx`).
- `src/Window.ixx`: exports partition `VEEngine:Window`; defines `vve::WindowSetup`, `vve::WindowSetups`, `vve::Key`, `vve::InputState`, `vve::DefaultCameraController`, `vve::Window`, and `vve::WindowSystem`.
- `src/Assets.ixx`: exports partition `VEEngine:Assets`; defines `vve::AssetSystem`.
- `src/RenderSystem.ixx`: exports partition `VEEngine:RenderSystem`; defines `vve::RenderSystem` and `vve::RenderShadowDepthSample`. It includes `VVPPL.h`, because `RenderSystem::setPostProcessSetup` takes a `std::function<void(vvppl::PostProcessing &)>`.
- `src/Gui.ixx`: exports partition `VEEngine:Gui`; defines `vve::GuiSystem`.
- `src/Types.ixx`: exports module `VEEngine.Types`; defines facade handles (`vve::SceneHandle`, `vve::RenderObjectHandle`, `vve::RenderSceneInstanceHandle`, ...), strong types, light and scene-object descriptors (`vve::DirectionalLight`, `vve::PointLight`, `vve::SpotLight`, `vve::PlaneDescriptor`, `vve::CuboidDescriptor`, `vve::TexturedCuboidDescriptor`, `vve::LightDescriptor`, `vve::CameraDescriptor`, `vve::SceneInstantiationOptions`), `vve::ApplicationName`, `vve::MaxFrames`, `vve::EngineConfig`, `vve::FrameContext`, `vve::FrameStatus`, `vve::Transform`, `vve::Bounds`, and `vve::Camera`; re-exports `vve::Entity` from `VEEngine.Entity` (`src/Entity.ixx`).
- `src/Math.ixx`: exports module `VEEngine.Math`; defines facade math aliases and functions in `vve::math` plus selected aliases in `vve`.
- `src/Handle.ixx`: exports module `VEEngine.Handle`; defines `vve::TypedHandle<TTag>`, `vve::HandleHash`, and the handle factory helpers.
- `src/Vector.ixx`: exports module `VEEngine.Vector`; defines `vve::Vector<T, SegmentSize>`.
- `src/Error.ixx`: exports module `VEEngine.Error`; defines `vve::Error` and `vve::errorName`.

Examples directory: `examples/`.

## Audit criteria

An example passes when it: imports only `VEEngine` (plus `std`) from the engine; uses only `vve::` facade symbols (no `vve::simple`, `vve::detail` or other implementation namespaces); includes no internal engine headers. Third-party headers exposed by design (e.g. `imgui.h`, consumed through `vve::GuiSystem::draw`, and `VVPPL.h`, consumed through `vve::RenderSystem::setPostProcessSetup`) are allowed.

## Example source inventory

### `examples/game/game.cpp` — PASS

Engine imports: `VEEngine` only. Headers: `imgui.h` (third-party, used inside the `vve::GuiSystem::draw` callback — the intended facade GUI pattern).

Facade symbols used: `vve::EngineBuilder`, `vve::WindowSetup`, `vve::RenderSystem`, `vve::WindowSystem`, `vve::GuiSystem`, `vve::DefaultCameraController`, `vve::Camera`, `vve::Key`, `vve::Direction`, `vve::Position`, `vve::LinearColor`, `vve::LightIntensity`, `vve::Transform`, `vve::RenderObjectHandle`, `vve::Vec3`, `vve::Vector`, `vve::PixelExtent`, `vve::RendererId`, `vve::FrameStatus`, `vve::Error`, `vve::errorName`, `vve::math`, `vve::engineImplementationNamespaceName`.

### `examples/light_shadow_debug/light_shadow_debug.cpp` — PASS

Engine imports: `VEEngine` only. No internal headers; shadow diagnostics use the facade shadow-depth sample queries (`vve::RenderSystem::setShadowDepthReadback`, `shadowDepthSamples`) and the PNG is written through `captureFrameToPng`.

Facade symbols used: `vve::EngineBuilder`, `vve::WindowSetup`, `vve::RenderSystem`, light/scene descriptor types (`vve::Direction`, `vve::Position`, `vve::LinearColor`, `vve::LightIntensity`, `vve::LightRange`, `vve::SpotConeAngle`, `vve::Transform`), `vve::Vec2`, `vve::Vec3`, `vve::PixelExtent`, `vve::RendererId`, `vve::FrameStatus`, `vve::Error`, `vve::errorName`, `vve::engineImplementationNamespaceName`.

### `examples/physics/physics.cpp` — PASS

Engine imports: `VEEngine` only.

Facade symbols used: `vve::EngineBuilder`, `vve::WindowSetup`, `vve::RenderSystem` (`loadSampleScene`), `vve::PixelExtent`, `vve::RendererId`, `vve::FrameStatus`, `vve::errorName`, `vve::engineImplementationNamespaceName`.

### `examples/postprocessing/postprocessing.cpp` — PASS

Engine imports: `VEEngine` only. Headers: `imgui.h` (used inside the `vve::GuiSystem::draw` callback) and `VVPPL.h` (third-party post-processing library; the effect chain is configured in the callback passed to `vve::RenderSystem::setPostProcessSetup`).

Facade symbols used: `vve::EngineBuilder`, `vve::WindowSetup`, `vve::RenderSystem` (`setPostProcessSetup`, `renderingFramesPerSecond`), `vve::WindowSystem`, `vve::GuiSystem`, `vve::DefaultCameraController`, `vve::Key`, `vve::Direction`, `vve::Position`, `vve::LinearColor`, `vve::LightIntensity`, `vve::LightRange`, `vve::Transform`, `vve::Vec2`, `vve::Vec3`, `vve::PixelExtent`, `vve::RendererId`, `vve::FrameStatus`, `vve::Error`, `vve::errorName`, `vve::math`, `vve::engineImplementationNamespaceName`.

### `examples/simple_forward_demo/simple_forward_demo.cpp` — PASS

Engine imports: `VEEngine` only. Frame capture goes through `vve::RenderSystem::captureFrameToPng`.

Facade symbols used: `vve::EngineBuilder`, `vve::WindowSetup`, `vve::RenderSystem` (`loadSampleScene`, `captureFrameToPng`, `renderedFrameCount`, `renderingFramesPerSecond`), `vve::PixelExtent`, `vve::RendererId`, `vve::FrameStatus`, `vve::Error`, `vve::errorName`, `vve::engineImplementationNamespaceName`.

### `examples/sponza/sponza.cpp` — PASS

Engine imports: `VEEngine` only.

Facade symbols used: `vve::EngineBuilder`, `vve::WindowSetup`, `vve::AssetSystem` (`loadScene`), `vve::RenderSystem` (`instantiateScene`, `setCamera`), `vve::WindowSystem`, `vve::DefaultCameraController`, `vve::Key`, `vve::SceneHandle`, `vve::RenderSceneInstanceHandle`, `vve::SceneInstantiationOptions`, `vve::Position`, `vve::Vec3`, `vve::PixelExtent`, `vve::RendererId`, `vve::FrameStatus`, `vve::Error`, `vve::errorName`, `vve::math`, `vve::engineImplementationNamespaceName`.

### `examples/testscene/testscene.cpp` — PASS

Engine imports: `VEEngine` only. Headers: `imgui.h` (used inside the `vve::GuiSystem::draw` callback).

Facade symbols used: `vve::EngineBuilder`, `vve::WindowSetup`, `vve::RenderSystem` (`set`/`add` directional, point and spot lights, `addPlane`, `addTexturedCuboid`, `setCamera`), `vve::WindowSystem`, `vve::GuiSystem`, `vve::DefaultCameraController`, `vve::Key`, `vve::Direction`, `vve::Position`, `vve::LinearColor`, `vve::LightIntensity`, `vve::LightRange`, `vve::SpotConeAngle`, `vve::Transform`, `vve::Vec2`, `vve::Vec3`, `vve::PixelExtent`, `vve::RendererId`, `vve::FrameStatus`, `vve::Error`, `vve::errorName`, `vve::math`, `vve::engineImplementationNamespaceName`.

## Verification

Checked with: `grep -rn "vve::simple\|vve::detail\|VEEngine\.Simple\|VVE_SDL_VULKAN_LIBRARY\|backend()\|VulkanReadback" examples/ --include=*.cpp` → zero matches. Every example's engine imports are exactly `import std;` and `import VEEngine;`. Each example target links only `ViennaVulkanEngine::ViennaVulkanEngine` (plus `imgui::imgui` for `game`, `postprocessing` and `testscene`).
