# AGENTS.md

If available, read CLAUDE.md.

## Purpose

This repository is building a reusable, ECS-first game meta engine in modern C++23.
The meta engine is meant for educational purposes, used in book projects or in lectures
and student projects. 

## Architecture

The meta engine is a container for concrete independent game engines and offers a common interface to 
user programmed games. If games restrict themselves to this common interface, then the meta engine
can be upgraded in the future to contain newer more modern game engines, and the
games then can opt to use them and still work without source code changes.

## Implementation Principles

The compiled outcome is a meta game engine, referred to as meta engine. It is a container for engine implementations, each being isoltaed from each other.

The common interface is defined in the src folder and does not contain any implementation itself, just interface contracts defining a facade facing towards the user program. This is the facade layer. The facade layer lives in namespace vve. The exception is the shared vocabulary that implementations import instead of duplicating it: the modules VVEngine.Math, VVEngine.Error, VVEngine.Handle, VVEngine.Entity, VVEngine.Vector, VVEngine.Types and VVEngine.ECSContainer (the simple engine re-exports them through src/versions/simple/Types.ixx).

User programs should only call into the facade layer. User programs are not allowed, under no circumstances, to use any detail of any concrete engine, directly. All interactions with the meta engine must be done via the official engine facade.

The engine implementations are situated in the src/versions folder and are completely isolated from each other. This is the implementation layer. The meta engine is compiled with exactly one engine implementation. Each game engine is isolated with its own namespace. For instance, the simple engine lives in the namespace vve::simple, source files are located in folder src/versions/simple.

When compiling their game, game apps are compiled and linked against the meta engine. Which engine implementation is used is selected at configure time by the CMake variable VVE_ENGINE_IMPLEMENTATION_NAMESPACE. It names one file src/implementations/<name>.ixx (the module partition VVEngine:Implementation) and one directory src/versions/<name>/. The partition is the only facade file that imports the implementation module; it exports type aliases vve::detail::EngineImpl, AssetSystemImpl, RenderSystemImpl, WindowSystemImpl, WindowImpl, InputStateImpl, GuiSystemImpl, WindowsImpl plus vve::engineImplementationNamespaceName. No other facade file may name a concrete implementation namespace. Currently only the simple engine exists; earlier implementations (v3, v4, v5) were removed as dead code.

Facades are defined in a facade pattern through wrapper classes and functions. Every class that is seen by the user lives in the facade layer as a wrapper. Wrappers have exactly one private member variable impl_ which is of type 
```cpp
  using Impl = detail::<WRAPPED_CLASS>Impl;
  Impl &impl_;
```
where the alias is provided by the selected src/implementations/<name>.ixx and refers to a specific class of the implementation layer. Read-only views hold `const Impl &impl_` (Window).
Wrappers mimic each method of the implementation, receive the same parameters and then forward them to the implementation. This way the contract is enforced and restricted to the allowed interface.

A facade wrapper class should be one class declaration and function definitions at the same time.
All classes in the implementation layer should first make a full class declaration only. In a file, these declarations come at the begin. After the class declarations, the member function definitions follow. 

Do not duplicate code. There is one system responsible and it has the only implementation for its functionality. Wrappers can forward calls to them.

Keep the file structure lean and simple, keep number of files as small as possible, but do not overload single files. ideally file sizes schon not exceed 500 LOC.

Keep the number of structs and classes at a minimum. Try to create few general templated solutions and derive special solutions from them. For instance, containers like trees or graphs, etc.

## Facade Structure

Engine implementations must expose specific subsystems with the enforced interface.

- Handle: Handles are strongly typed wrappers around uint64_t and id any resource.
- Math: Provides all math related functions.
- Error: the `vve::Error` enum and `errorName()`. Fallible calls return `std::expected<T, Error>`; enum values represent errors the engine produces. Repeated window initialization returns `already_initialized`, unresolved texture paths return `io_error`, and failed Assimp imports return `asset_import_failed`.
- Vector: `Vector<T>` aliases `std::vector<T>`; the facade API passes contiguous sequences as `Vector`. Growth may invalidate element pointers, references and iterators; reserve first or look up elements again after growth.
- Engine: This is created by the user app (`Engine<TSystems...>`, or `EngineBuilder`) and handles main frame events: `init()`, `step()`, `run()`, calling the optional `init(world)` and `update(world[, frame[, window_frame]])` hooks of the user systems. It owns its own Engine implementation through `std::unique_ptr<detail::EngineImpl>`, created by the inline non-template `detail::makeEngineImpl` factory in src/Engine.ixx, holds the AssetSystem, GuiSystem, WindowSystem and RenderSystem wrappers, and returns a World by value from `world()`. Startup options are ApplicationName, MaxFrames, WindowSetups and UserSystems; any other option type is a compile error. Without a UserSystems option the user systems are default-constructed (a system without a default constructor is a compile error then). If a user-system init hook fails, `init()` returns its error; retries resume at that system without repeating successful hooks. Systems without an init hook count as initialized.
- ECS: an Entity Component System that can hold entities with any data type. It is facade-owned (src/ECSContainer.ixx) and shared with the implementation, not a wrapper.
- WindowSystem: a window manager that holds all windows in a container.  There is a WindowSystem wrapper in the facade, and an implementation. The WindowSysten implementation owns the Window implementations, and can return Window wrappers. It also hands out the InputState wrapper.
- Window: read-only window view: handle, id, title, extent (drawable pixels), renderer id, focused/minimized/should-close state. Cameras belong to RenderSystem: `setCamera(Camera)` sets the default, `setCamera(WindowHandle, Camera)` overrides one window and `clearCamera(WindowHandle)` restores its use of the current default. Each window supplies its own aspect ratio; the selected camera's `ClipPlanes` (default near 0.1, far 100) drive projection and directional cascades. Camera stores position, forward direction, field of view and clip planes. Windows and frame snapshots carry no ECS camera binding. VVEngine.Types defines the shared WindowDesc, WindowInfo and WindowFrameData; WindowSetup wraps a WindowDesc.
- Assets: A wrapper over the internal asset system. The wrapper exposes specific public functions that enable users to load scene files from disk (`loadScene`), add empty scenes and query scenes, nodes, meshes, materials, lights and cameras. `sceneTextureCount` counts distinct texture source paths referenced by the scene's materials. `sceneNodes` follows tree creation (pre-order) order; `nodeMaterials` follows each attached mesh's material, and mesh counts come from their arrays. Imported `meshBounds` remain available. Facade asset queries keep owning by-value results; simple-engine catalog pointers and `mesh*View()` spans are borrowed only until catalog mutation or destruction. Object creation and purging are RenderSystem functions.
- RenderSystem: wrapper over the render system implementation: lights (`clearLights` removes all lights while preserving objects and scene instances), camera, procedural objects (plane, cuboid, triangle mesh, textured cuboid), object state, scene instantiation and removal (`instantiateScene`, `removeSceneInstance`, `removeScene`), `purgeUnusedAssets`, scene statistics, shadow-depth samples (the shared RenderShadowDepthSample in VVEngine.Types), PNG frame capture and the vvppl post-processing setup.
- Primitive objects share materials by base colour and texture slot; planes and cuboids share geometry by shape and local extents. Place objects through `Transform`. `removeObject` releases unreferenced non-imported meshes/materials and unused textures; imported caches survive until explicit purge or scene removal. `RenderSystem::sceneTextureCount()` counts live render texture slots. Texture decode failures remain cached until `clearScene()`; repairing a file requires clearing before retrying.
- GUI: wrapper over the GUI system implementation. The wrapper offers public hooks for creating widgets (`draw(std::function<void()>)`). Must work with ImGUI. 
- World: main interface for user interaction with the world and runtime binder.
  - World (`World<TObjects...>`, src/World.ixx) does not store data itself but holds references to other subsystems.
  - World returns these references to the user app with `get<T>()`: the ECS, the AssetSystem, GuiSystem, WindowSystem and RenderSystem wrappers, and the user systems.
  - Through these wrappers users access and change world, scene and asset data without seeing internal descriptor types like the simple engine's Catalog or AssetScene. 
  - There is no World implementation class; World is a facade class template.
  - A value of it is obtained through `Engine::world()`. It can be move-constructed but not copied or assigned.
  - The facade Engine owns its own engine implementation. The subsystem wrappers it holds are non-owning references to implementation objects owned by that engine implementation.

Additionally the facade defines numerous low level structs for storing pure data, like Position, 
Direction, Transform, LinearColor, etc. These must be used in the engine implementations accordingly and are typical data structures used in game engines.

Internal data are not part of the facade. The user states what he wants, 
the engine decides how this is done without exposing details about internal implementation. This also involves the containers storing these descriptors. 

Examples for internal data structures are (names in the simple engine in brackets)
- ObjectCatalogues (Catalog in src/versions/simple/Assets.ixx)
- SceneDescriptors (AssetScene; RenderScene in src/versions/simple/Render/RenderResources.ixx)
- NodeDescriptors used for creating internal DAGs or trees (Node, SceneTree over the Graph/Tree helpers in src/versions/simple/Graph.ixx). Graph membership uses a handle set plus an order vector; names belong to the asset nodes, not the graph.

Descriptors that might be exposed to the user must be composed by facade defined data types only. However, as a general principle, user intercation should be via functions, not data structures.

## Strong Types

- Prefer strong types over raw primitives where semantics matter.
- Encode units, identifiers, handles, indices, ranges, states, flags, and categories as explicit types when practical.
- Avoid ambiguous `int`, `float`, `bool`, `string`, or loosely structured parameter lists in important APIs.
- Use types to prevent category mistakes, invalid combinations, and accidental misuse.
- Strong types should improve correctness without creating excessive ceremony on hot paths.
- Distinguish public semantic types from internal storage-efficient representations where needed.
- The name of a strong type should reflect its semantic meaning.

## External Depenencies

The engine mainly links to the official Vulkan SDK and uses the libraries contained there. Additonally it may use libraries like SDL3, Assimp, STB, ImGUI. These external libraries are downloaded using vcpkg.

IMPORTANT: On Mac you should use KosmicKrisp, not MoltenVk.

### Code Documentation

- Add extensive Doxygen compatible comments to the code.
- Each file, function, class, struct gets a header explaining why it is there and if necessary input, output and return parameters.
- In a struct or enum or class, each member variable or value must have its own comment line at the end. 
- Function declarations should not have function header comments, only definitions.
- When adding comments, try to minimize the number of lines in the file. If max line length allows, put comments in the same line after the code.
- Add comments to roughly 30 to 50 percent of all code lines.
- If a new code block, e.g., a loop, begins, add comments in front of it to explain what the following code does.
- Keep comments simple and abstract. Prefer "what" to "how".
- In sequential lines, try to align comments vertically by adding tabs. 

### Modern C++ first

- Prefer modern C++ and the standard library by default.
- Use STL containers, algorithms, ranges, utilities, ownership models, `span`, `optional`, `variant`, `string_view`, concepts, and constexpr-oriented design where appropriate.
- Prefer standard facilities before introducing custom equivalents.
- Only replace STL or standard patterns when profiling, memory layout needs, platform constraints, or API constraints justify it.
- Optimize after understanding real hot paths, but design hot-path architecture correctly from the start.
- Prefer idiomatic modern C++ over C-style patterns or legacy inheritance-heavy OOP.
- Use templates deliberately, not decoratively.
- Prefer range-based for-loops over counted for-loops.

## Expectations for code generation

- Keep the number of lines as low as possible and feasible. 
- Do not bloat, do not introduce new classes or structs without asking.
- Do not violate the meta engine facade to engine implementation rules.
- Do not introduce layers of abstraction without asking.
- Do not introduce functionality that is already covered by std.
- Keeop the code readable, slim, expressive.
- Always document the code.
- If in doubt, ask. 
- Avoid if sequences if there are many choices. Prefer constexpr data structures that selectors 
can be used to index into.
- Do not duplicate code. Instead create abstractions or templates that can be reused.

## Expectations for Examples

Examples are implemented in the examples folder. The showcase a certain aspect of rendering, use the official API
of the facade, and do not depend on anything specific to a specific engine. Instead they must work
for all engines.

## Expectations for debugging

Always create testable executable that produce deterministic output. This output can be text, numbers, names, flags, or images e.g. PNG.
The controller should always let the worker create these tests, then compile them. Here the controller should already examine whether compilation succeeded.
If not, the controller should issue a tasl to the worker to resolve the compilation issues.
Once all compilations have succeeded, the controller should execute them with the appropriate parameters and observe the output. 
If the output is not what was exepcted, the controller shouls issue a repair task to the worker to analyse and fix the propblem.
Do not rely on Python for anything in any engine. Testng should rely on C++ tests.
Always include a lighweight data layer for carrying small sets of debugging information that can be checked automatically by an LLM later. 

## Expectations for tests

All tests and example programs must compile without error. If compile errors are detected, solve them. If test programs report that they failed then analyse the output and solce the error. 
All classes and functions should have unit tests.
All example programs should have extensive test paths.
Tests should produce debugging data that lets a calling LLM detect errors, locate errors and fix errors.
Executables targeted towards a certain overal goal like rendering a test scene should be callable with various parameters and produce deterministic output.
For example when testing rendering with various lighst, the test program could be run several times, each time using a different light. The output could be either text detailing internals, and an image rendered with the light.

## Build and test

- `./build_linux.sh [debug|release] [--clean]`, `./build_macos.sh [debug|release] [--clean]` and `.\build_windows.cmd [debug|release] [--clean] [--no-tests] [--docs]` configure, build and run CTest (default variant: release). Build directories: `build/<variant>-linux`, `build/macos-<variant>`, `build\<variant>-windows`; the CMake presets use `build/<preset name>`. All builds use Ninja, because CMake supports `import std` only with Ninja generators; the Windows presets need a Developer PowerShell for Visual Studio. See README.md for prerequisites.
- Executables are written to `bin/<variant>/exe`. The light_shadow_debug example writes `bin/<variant>/verify/light_shadow_debug.txt` and `.png`.
- Tests are C++ executables in the tests folder, one file each, that return 0 on success. Each must be registered in tests/CMakeLists.txt with `vve_add_engine_test(<Name> [extra libraries])`; an unregistered file is never built or run. CTest also runs the examples as `LightShadowDebugExample` and `PostProcessingSmokeTests`.
- Rerun tests with `ctest --test-dir <build directory> --output-on-failure [-R <name>]`.
- Shared test helpers live in `tests/support/TestSupport.ixx` (module `VVE.TestSupport`, namespace `vve::test`); importing tests link `vve_test_support`. It provides hidden simple-engine setup, text fixtures and RGBA capture measurements. Keep test-specific fixtures and assertions in the individual tests.

# Concrete Engines

Concrete engines are defined in the respective subfolder of src/versions. Each subfolder contains its own engine and AGENTS.md.
Ignore the AGENTS.md files of other engines, only read and analyse the AGENTS.md of the respective folder that contains the engine under consideration.
Engines must be completely isolated from each other. One engine must never refer to another engine, or use something from another engine.
If instructions say: use this from another engine, then do not make an alias etc ro anything of this engien. Instead create new functionality for the concrete engine but make it similar to the functionality of the referred engine.

At configure time, the CMake cache variable VVE_ENGINE_IMPLEMENTATION_NAMESPACE (default and currently only value: simple; set by the presets and build scripts) selects one of the engines available. 

## src/versions/simple

The simple engine should be the bare engine minimum. Keep the number of lines at a minimum. Always analyse this engine and find ways to make smarter code with less lines of code, templates, code reusing, etc. Make the code simple and easy to understand for human beings. Keep functions and classes simple with single focus and intent.
