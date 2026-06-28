## Context
The repository is a reusable ECS-first C++23 meta engine for educational game projects.
The public facade lives in namespace `vve`, while concrete engines live isolated under `src/versions`; user code should call only facade wrappers.
Facade wrappers forward to the selected implementation namespace through `VVE_ENGINE_IMPLEMENTATION_NAMESPACE` and should expose only the enforced common interface.
The required subsystems include Handle, Math, Vector, Engine, ECS, WindowSystem, Window, Assets, GUI, and World, with World delegating to subsystem implementations without exposing internal descriptors.
The design should stay lean, avoid duplicated responsibility, prefer strong semantic types where useful, and use modern C++ standard library facilities before custom solutions.
Code should be extensively but compactly documented with Doxygen-compatible comments, file overviews, function/class headers, and member comments.
The simple engine is the bare minimum engine: render objects with lights and shadows.
It should use a v5-like scene graph, reuse v5 helpers such as Handle, Vector, Graph, ECS, and Math, and avoid render graphs, task graphs, and virtual layers.
It should mirror the Khronos Vulkan Tutorial multiple-objects structure and use SDL3, VMA, Assimp, Slang, dynamic rendering, Vulkan profiles, and Vulkan Specification 1.4.
It should support asset loading and rendering multiple objects, plus a minimal debugging layer for Slang, SDL3 windows, and rendering output.
The light-shadow debug example from v5 should produce known PNG-checkable output so an LLM can analyze whether results are correct, while GPU-to-CPU debug data stays minimal.
The simple engine should avoid Python entirely, including tests, and may use cmd or bash scripts.

## Open Questions
Which v5 helper APIs are considered stable enough to reuse directly for the simple engine?
Where should the known light-shadow PNG reference output be stored, and what exact comparison tolerance is expected?
How strictly should the simple engine follow the Vulkan Tutorial file/module structure when that conflicts with the repository facade and subsystem layout?
Which Vulkan profile and SDL3/VMA/Assimp/Slang versions are expected for the first simple-engine target?

## Implementation Steps
1. `src/versions/simple/Handle.ixx`: define the simple engine handle plan around the reused v5-style strong `uint64_t` handle helper; stop when handle identity and ownership boundaries are specified without adding storage logic.
2. `src/versions/simple/Vector.ixx`: plan the reused v5-style vector container interface with iterator expectations; stop when the facade-visible container behavior is mapped to the helper without custom container requirements.
3. `src/versions/simple/Math.ixx`: plan the reused v5-style math surface for transforms, camera data, lights, and shadows; stop when the required math data passed between subsystems is named and no renderer code is specified.
4. `src/versions/simple/ECS.ixx`: plan the minimal ECS implementation that can hold entities with arbitrary component data; stop when entity/component responsibilities are separated from scene graph and asset ownership.
5. `src/versions/simple/Graph.ixx`: plan reuse of the v5-style graph helper as the generic graph basis; stop when the graph helper role is limited to supporting the scene graph and no render or task graph is introduced.
6. `src/versions/simple/SceneGraph.ixx`: plan a v5-like scene graph for multiple renderable objects; stop when object hierarchy, transform propagation, and light/shadow placement responsibilities are assigned without exposing internal descriptors.
7. `src/versions/simple/Window.ixx`: plan the minimal SDL3-backed window information object for size, renderer-facing state, and camera association; stop when the window data needed by the window system and world is identified.
8. `src/versions/simple/WindowSystem.ixx`: plan the window manager that owns window implementations and returns window access through the facade contract; stop when SDL3 window lifecycle responsibilities are isolated from rendering and world logic.
9. `src/versions/simple/Assets.ixx`: plan the asset subsystem for disk loading, purging, object creation, and Assimp-backed multiple-object input; stop when user-facing asset functions are listed without exposing object catalogues or descriptors.
10. `src/versions/simple/GUI.ixx`: plan the minimal GUI hooks required by the facade and ImGUI expectation; stop when GUI responsibilities are limited to user hooks and debug presentation points.
11. `src/versions/simple/LightShadowDebug.ixx`: plan the minimal debug data path for Slang, SDL3 windows, rendering output, and known PNG-checkable light-shadow results; stop when the CPU-visible debug data is constrained to the minimum needed for analysis.
12. `src/versions/simple/World.ixx`: plan the world implementation as references to ECS, Assets, GUI, Engine, and WindowSystem plus facade-safe forwarding functions; stop when all subsystem access is available without storing world data or exposing internal descriptors.
13. `src/versions/simple/Engine.ixx`: plan the engine owner that constructs subsystem implementations, follows the Vulkan Tutorial multiple-objects frame structure with Vulkan 1.4, SDL3, VMA, Slang, dynamic rendering, and Vulkan profiles, and returns a World value; stop when startup, frame, and shutdown responsibilities are assigned without implementing engine code.
