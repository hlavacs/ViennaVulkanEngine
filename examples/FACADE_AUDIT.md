# Facade API Audit

## Public facade API

The official application-facing API is the facade module `VEEngine`, exported from `src/Engine.ixx`. User code should import `VEEngine` and use namespace `vve`, not implementation namespaces such as `vve::simple`.

Facade source files:

- `src/Engine.ixx`: exports module `VEEngine`; defines `vve::Engine<TSystems...>` and `vve::EngineBuilder<TSystems...>`.
- `src/World.ixx`: exports partition `VEEngine:World`; defines `vve::World<TObjects...>`, `vve::UserSystems<TSystems...>`, and `vve::makeUserSystems`.
- `src/ECS.ixx`: exports partition `VEEngine:ECS`; defines `vve::BasicECS<TTraits>` and `vve::ECS`.
- `src/Window.ixx`: exports partition `VEEngine:Window`; defines `vve::WindowSetup`, `vve::WindowSetups`, `vve::InputState`, `vve::Window`, and `vve::WindowSystem`.
- `src/Assets.ixx`: exports partition `VEEngine:Assets`; defines `vve::AssetSystem`.
- `src/RenderSystem.ixx`: exports partition `VEEngine:RenderSystem`; defines `vve::RenderSystem`, `vve::RenderDebugSample`, and `vve::RenderShadowDepthSample`.
- `src/Gui.ixx`: exports partition `VEEngine:Gui`; defines `vve::GuiSystem`.
- `src/Types.ixx`: exports module `VEEngine.Types`; defines facade handles, strong types, `vve::EngineConfig`, `vve::FrameStatus`, `vve::Transform`, `vve::Bounds`, and `vve::Camera`.
- `src/Math.ixx`: exports module `VEEngine.Math`; defines facade math aliases and functions in `vve::math` plus selected aliases in `vve`.
- `src/Handle.ixx`: exports module `VEEngine.Handle`; defines `vve::TypedHandle<TTag>`.
- `src/Vector.ixx`: exports module `VEEngine.Vector`; defines `vve::Vector<T>` and `vve::VectorConstRange<T>`.
- `src/Error.ixx`: exports module `VEEngine.Error`; defines `vve::Error` and `vve::errorName`.

Examples directory: `examples/`.

## Example source inventory

### `examples/game/game.cpp`

Internal engine headers included: none. The file includes only SDL and Vulkan headers.

Internal engine module imports:

- `VEEngine.Simple.Math`
- `VEEngine.Simple.Mesh`
- `VEEngine.Simple`
- `VEEngine.Simple.Scene`

Non-facade engine symbols/types used:

- `vve::simple::DirectionalLight`
- `vve::simple::Object`
- `vve::simple::PointLight`
- `vve::simple::RenderSystem`
- `vve::simple::Scene`
- `vve::simple::SpotLight`
- `vve::simple::Vec2`
- `vve::simple::Vec3`
- `vve::simple::add`
- `vve::simple::clamp`
- `vve::simple::cross`
- `vve::simple::identityMat4`
- `vve::simple::makeCube`
- `vve::simple::makePlane`
- `vve::simple::normalize`
- `vve::simple::scale`
- `vve::simple::subtract`
- `vve::simple::translate`
- Internal `vve::simple::RenderSystem` members: `loadScene`, `initialize`, `scene`, `setCamera`, `drawFrame`, and `shutdown`.
- Internal scene/light fields reached through `renderSystem.scene()`: `spotLight`, `pointLight`, `directionalLight`, `intensity`, `ambient`, and `range`.
- Engine-supplied preprocessor symbol `VVE_SDL_VULKAN_LIBRARY`.

### `examples/light_shadow_debug/light_shadow_debug.cpp`

Internal engine headers included: none. The file includes only SDL and Vulkan headers.

Internal engine module imports:

- `VEEngine.Simple.Math`
- `VEEngine.Simple.Mesh`
- `VEEngine.Simple`
- `VEEngine.Simple.Scene`
- `VEEngine.Simple.Vulkan`

Non-facade engine symbols/types used:

- `vve::simple::DirectionalLight`
- `vve::simple::Mesh`
- `vve::simple::Object`
- `vve::simple::PointLight`
- `vve::simple::RenderSystem`
- `vve::simple::Scene`
- `vve::simple::SpotLight`
- `vve::simple::Vec2`
- `vve::simple::Vec3`
- `vve::simple::VulkanReadback`
- `vve::simple::identityMat4`
- `vve::simple::makeCube`
- `vve::simple::makePlane`
- `vve::simple::scale`
- `vve::simple::translate`
- `vve::simple::writeReadbackPng`
- Internal `vve::simple::Mesh` fields: `vertices` and vertex `color`.
- Internal `vve::simple::RenderSystem` members: `loadScene`, `initialize`, `backend`, `drawFrame`, and `shutdown`.
- Internal renderer backend fields reached through `renderSystem.backend()`: `physicalDevice.physicalDevice`, `device.device`, `device.graphicsQueue`, `commandPool.commandPool`, `swapchain.extent`, `swapchain.imageFormat`, and `lastReadbackCaptureResult`.
- Internal `vve::simple::VulkanReadback` members: `create`, `pixelBytes`, and `cleanup`.
- Internal scene/light fields: `objects`, `pointLight`, `directionalLight`, `spotLight`, `position`, `direction`, `color`, `intensity`, `range`, `ambient`, `innerConeAngle`, and `outerConeAngle`.
- Engine-supplied preprocessor symbol `VVE_SDL_VULKAN_LIBRARY`.

### `examples/physics/physics.cpp`

Internal engine headers included: none. The file includes only SDL and Vulkan headers.

Internal engine module imports:

- `VEEngine.Simple`
- `VEEngine.Simple.Scene`

Non-facade engine symbols/types used:

- `vve::simple::RenderSystem`
- `vve::simple::makeSampleScene`
- Internal `vve::simple::RenderSystem` members: `loadScene`, `initialize`, `drawFrame`, and `shutdown`.
- Engine-supplied preprocessor symbol `VVE_SDL_VULKAN_LIBRARY`.

### `examples/simple_forward_demo/simple_forward_demo.cpp`

Internal engine headers included: none. The file includes only SDL and Vulkan headers.

Internal engine module imports:

- `VEEngine.Simple`
- `VEEngine.Simple.Scene`
- `VEEngine.Simple.Vulkan`

Non-facade engine symbols/types used:

- `vve::simple::RenderSystem`
- `vve::simple::VulkanReadback`
- `vve::simple::makeSampleScene`
- `vve::simple::writeReadbackPng`
- Internal `vve::simple::RenderSystem` members: `loadScene`, `initialize`, `backend`, `drawFrame`, and `shutdown`.
- Internal renderer backend fields reached through `renderSystem.backend()`: `physicalDevice.physicalDevice`, `device.device`, `device.graphicsQueue`, `commandPool.commandPool`, `swapchain.extent`, `swapchain.imageFormat`, and `lastReadbackCaptureResult`.
- Internal `vve::simple::VulkanReadback` members and fields: `create`, `pixelBytes`, `cleanup`, `buffer.buffer`, and `buffer.memory`.
- Engine-supplied preprocessor symbol `VVE_SDL_VULKAN_LIBRARY`.

### `examples/sponza/sponza.cpp`

Internal engine headers included: none. The file includes only SDL and Vulkan headers.

Internal engine module imports:

- `VEEngine.Simple`
- `VEEngine.Simple.Scene`

Non-facade engine symbols/types used:

- `vve::simple::RenderSystem`
- `vve::simple::makeSampleScene`
- Internal `vve::simple::RenderSystem` members: `loadScene`, `initialize`, `drawFrame`, and `shutdown`.
- Engine-supplied preprocessor symbol `VVE_SDL_VULKAN_LIBRARY`.
