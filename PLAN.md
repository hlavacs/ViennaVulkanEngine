Read AGENTS.md and src/versions/simple/AGENTS.md

  Implement support for multiple directional lights in the simple engine, following the current multi-spot-light pattern.

  Current state: the simple scene has one DirectionalLight slot, while spot lights have a capped vector using kMaxShadowedSpotLights and addSpotLight
  APIs. Extend this model for directional lights with a small fixed cap, preferably 4, without exposing implementation namespaces to user code.

  Requirements:
  1. Add a simple-engine cap such as kMaxDirectionalLights in src/versions/simple/Scene.ixx.
  2. Change the simple Scene model to store a vector of DirectionalLight entries while preserving the existing single directionalLight mirror for
  compatibility with current renderer code.
  3. Add RenderScene::addDirectionalLight and corresponding facade-level RenderSystem::addDirectionalLight overloads in src/RenderSystem.ixx and src/
  RenderSystem.cpp.
  4. Keep setDirectionalLight as the legacy replacement API: it should clear existing directional lights and set the first one.
  5. Update RenderSystem::loadScene and forwarding code so directional-light vectors are clamped to the cap and the first light remains mirrored into
  the legacy directionalLight slot.
  6. Extend FrameUniforms in src/versions/simple/Vulkan.ixx to carry arrays for directional light directions, colors/intensities, ambient values, and an
  active count, matching the style used for multiple spot lights.
  7. Update src/versions/simple/Renderer.ixx to fill those arrays from the active directional-light vector each frame.
  8. Update the simple forward shader to sum contributions from all active directional lights. Preserve existing behavior when only one directional
  light is present.
  9. Keep the implementation compact. Do not add virtual layers, new engine namespaces, or broad abstractions.
  10. Update tests, especially SimpleForwardRendererTests or RenderSystemTests, to verify that multiple directional lights can be submitted, are capped,
  preserve first-light compatibility, and still build/run.
  11. Update examples/game/game.cpp to exercise multiple directional lights through the public vve facade, similar to the current multi-spot-light
  example UI.
  12. Run the macOS build/test command and ensure all tests pass.

  Acceptance:
  - User code can call setDirectionalLight for the old one-light behavior.
  - User code can call addDirectionalLight to add more directional lights up to the cap.
  - The simple forward renderer uses all active directional lights in lighting.
  - The public facade exposes only vve types.
  - ./build_macos.sh passes.
