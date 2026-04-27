v3 architecture is being shaped around these requirements:

**Core Architecture**
- Use clear subsystem boundaries: `WindowSystem`, `GraphicsBackend`, `ShaderSystem`, `ResourceSystem`, `SceneSystem`, `RenderSystem`, `TaskGraphSystem`.
- Public facades expose stable contracts; concrete implementations stay version-local.
- Keep Vulkan-specific objects private to `GraphicsBackend`; other systems should only see handles and summaries.
- Avoid workaround-style fixes. Each stage should compile, run examples, and remain architecturally clean.

**Flexibility**
- Renderer choice must be per window, not global.
- A window should accept a renderer ID like `forward`, `deferred`, or future renderers.
- Renderers must be replaceable without changing example code.
- The engine API should stay stable so examples can later run on `v3_minimal` or full `v3`.
- Platform-specific code must stay isolated, especially SDL/window creation and Vulkan surface creation.

**Separation Of Concern**
- `WindowSystem` owns SDL windows and input events.
- `GraphicsBackend` owns Vulkan instance/device/surfaces/swapchains/pipelines/frame sync.
- `RenderSystem` owns renderer selection, render graph structure, and per-window render tasks.
- `ShaderSystem` owns Slang compilation/reflection.
- `ResourceSystem` owns resource records and shader/resource lookup.
- `SceneSystem` owns instantiated scene graph/runtime scene data.
- Examples should not know Vulkan internals.

**Extendability**
- New renderers should be added by registering renderer descriptors and supplying matching render graph/pipeline behavior.
- New shader variants should flow through the resource/shader systems, not be loaded manually by examples.
- New frame work should become task-graph nodes, not ad-hoc calls in the engine loop.
- Swapchain resize, presentation, command recording, and future draw submission should remain replaceable pieces.

**Software Requirements**
- Must compile through the VS Code/CMake preset workflow.
- Must support macOS arm64 and Windows amd64.
- x86 macOS is no longer a target.
- `game`, `physics`, and `sponza` must keep launching successfully after every stage.
- All tests must pass after every stage.
- Dependencies should remain consistent and not create stray external install directories.

**Educational Requirements**
- v3 should be understandable as a staged engine architecture.
- Each subsystem should have a visible purpose.
- Data should move through explicit handles, summaries, and task graph edges.
- Graph dumps and diagnostics should make the frame architecture inspectable.
- The design should favor clarity over cleverness, while still being real Vulkan architecture rather than a toy wrapper.