# Facade API Audit

Status: **PASS** — all examples use only the public facade (audited 2026-07-04).

## Public facade API

The official application-facing API is the facade module `VEEngine`, exported from `src/Engine.ixx`. User code should import `VEEngine` and use namespace `vve`, not implementation namespaces such as `vve::simple`.

Facade source files:

- `src/Engine.ixx`: exports module `VEEngine`; defines `vve::Engine<TSystems...>` and `vve::EngineBuilder<TSystems...>`.
- `src/World.ixx`: exports partition `VEEngine:World`; defines `vve::World<TObjects...>`, `vve::UserSystems<TSystems...>`, and `vve::makeUserSystems`.
- `src/ECS.ixx`: exports partition `VEEngine:ECS`; defines `vve::BasicECS<TTraits>` and `vve::ECS`.
- `src/Window.ixx`: exports partition `VEEngine:Window`; defines `vve::WindowSetup`, `vve::WindowSetups`, `vve::InputState`, `vve::Key`, `vve::DefaultCameraController`, `vve::Window`, and `vve::WindowSystem`.
- `src/Assets.ixx`: exports partition `VEEngine:Assets`; defines `vve::AssetSystem`.
- `src/RenderSystem.ixx`: exports partition `VEEngine:RenderSystem`; defines `vve::RenderSystem`, `vve::RenderDebugSample`, `vve::RenderShadowDepthSample`, and `vve::SpotShadowDepthSample`.
- `src/Gui.ixx`: exports partition `VEEngine:Gui`; defines `vve::GuiSystem`.
- `src/Types.ixx`: exports module `VEEngine.Types`; defines facade handles, strong types, descriptors and builders, `vve::EngineConfig`, `vve::FrameStatus`, `vve::Transform`, `vve::Bounds`, and `vve::Camera`.
- `src/Math.ixx`: exports module `VEEngine.Math`; defines facade math aliases and functions in `vve::math` plus selected aliases in `vve`.
- `src/Handle.ixx`: exports module `VEEngine.Handle`; defines `vve::TypedHandle<TTag>`.
- `src/Vector.ixx`: exports module `VEEngine.Vector`; defines `vve::Vector<T>` and `vve::VectorConstRange<T>`.
- `src/Error.ixx`: exports module `VEEngine.Error`; defines `vve::Error` and `vve::errorName`.

Examples directory: `examples/`.

## Audit criteria

An example passes when it: imports only `VEEngine` (plus `std`) from the engine; uses only `vve::` facade symbols (no `vve::simple` or other implementation namespaces); includes no internal engine headers. Third-party headers exposed by design (e.g. `imgui.h`, consumed through `vve::GuiSystem::draw`) are allowed.

## Example source inventory

### `examples/game/game.cpp` — PASS

Engine imports: `VEEngine` only. Headers: `imgui.h` (third-party, used inside the `vve::GuiSystem::draw` callback — the intended facade GUI pattern).

Facade symbols used: `vve::EngineBuilder`, `vve::WindowSetup`, `vve::RenderSystem`, `vve::WindowSystem`, `vve::GuiSystem`, `vve::DefaultCameraController`, `vve::Key`, `vve::Direction`, `vve::Position`, `vve::LinearColor`, `vve::LightIntensity`, `vve::LightRange`, `vve::SpotConeAngle`, `vve::Transform`, `vve::PixelExtent`, `vve::RendererId`, `vve::FrameStatus`, `vve::Error`, `vve::errorName`, `vve::math`, `vve::engineImplementationNamespaceName`.

### `examples/light_shadow_debug/light_shadow_debug.cpp` — PASS

Engine imports: `VEEngine` only. No internal headers; renderer backend and readback internals formerly used here are replaced by facade shadow-depth sample queries and `vve::RenderSystem` scene setup.

Facade symbols used: `vve::EngineBuilder`, `vve::WindowSetup`, `vve::RenderSystem`, light/scene descriptor types (`vve::Direction`, `vve::Position`, `vve::LinearColor`, `vve::LightIntensity`, `vve::LightRange`, `vve::SpotConeAngle`, `vve::Transform`), `vve::PixelExtent`, `vve::RendererId`, `vve::FrameStatus`, `vve::Error`, `vve::errorName`, `vve::engineImplementationNamespaceName`.

### `examples/physics/physics.cpp` — PASS

Engine imports: `VEEngine` only.

Facade symbols used: `vve::EngineBuilder`, `vve::WindowSetup`, `vve::RenderSystem` (`loadSampleScene`), `vve::PixelExtent`, `vve::RendererId`, `vve::FrameStatus`, `vve::errorName`, `vve::engineImplementationNamespaceName`.

### `examples/simple_forward_demo/simple_forward_demo.cpp` — PASS

Engine imports: `VEEngine` only. Vulkan readback internals formerly used here are replaced by `vve::RenderSystem::captureFrameToPng`.

Facade symbols used: `vve::EngineBuilder`, `vve::WindowSetup`, `vve::RenderSystem`, `vve::PixelExtent`, `vve::RendererId`, `vve::FrameStatus`, `vve::Error`, `vve::errorName`, `vve::engineImplementationNamespaceName`.

### `examples/sponza/sponza.cpp` — PASS

Engine imports: `VEEngine` only.

Facade symbols used: `vve::EngineBuilder`, `vve::WindowSetup`, `vve::AssetSystem` (`loadScene`), `vve::RenderSystem` (`instantiateScene`), `vve::SceneHandle`, `vve::RenderSceneInstanceHandle`, `vve::SceneInstantiationOptions`, `vve::PixelExtent`, `vve::RendererId`, `vve::FrameStatus`, `vve::Error`, `vve::errorName`, `vve::engineImplementationNamespaceName`.

## Verification

Checked with: `grep -rn "vve::simple\|VEEngine\.Simple\|VVE_SDL_VULKAN_LIBRARY\|backend()\|VulkanReadback" examples/ --include=*.cpp` → zero matches. Every example's engine imports are exactly `import std;` and `import VEEngine;`.
