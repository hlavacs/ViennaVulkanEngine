# Vienna Vulkan Engine

## Setup

This project uses `vcpkg` manifest dependencies for third-party libraries: Assimp, GLM, ImGui, SDL3, SDL3_mixer, stb, and Vulkan Memory Allocator are declared in [vcpkg.json](vcpkg.json) and installed into the repo-local `vcpkg_installed` directory. SDL3 is built with its Vulkan feature enabled so the examples can create Vulkan-capable windows. The post-processing library vvppl ([ViennaVulkanPostProcessingLibrary](https://github.com/orcunilker/ViennaVulkanPostProcessingLibrary), commit `e69f3e56beca935ceb257cce6005f6e39847bcb4`) is not a vcpkg package: CMake downloads it with `FetchContent` as a pinned, SHA256 hash-verified archive during the first configure.

For offline builds, extract that archive beforehand and configure with `-DFETCHCONTENT_SOURCE_DIR_VIENNAVULKANPOSTPROCESSINGLIBRARY=<dir>`, where `<dir>` contains its top-level `CMakeLists.txt`. This uses the local source directory without downloading vvppl; the Vulkan SDK and vcpkg dependencies must also be installed beforehand.

The project expects Vulkan, Slang (`slangc` compiles and embeds the simple engine's shaders in the engine binary during the build; no runtime shader files are needed), and optional macOS Vulkan ICDs such as KosmicKrisp to come from the Vulkan SDK. CMake takes the SDK root from `VVE_VULKAN_SDK_ROOT` or `$ENV{VULKAN_SDK}`. If neither is set, it auto-detects SDK installs below `$HOME/VulkanSDK/*/macOS` on macOS and `$HOME/vulkansdk/*/x86_64` on Linux.

The simple renderer requires a Vulkan 1.3 device with `VK_KHR_swapchain`, `dynamicRendering` and `shaderSampledImageArrayDynamicIndexing`, plus graphics/compute and presentation queues. Vulkan 1.2 devices are rejected even with `VK_KHR_dynamic_rendering`. If no device qualifies, the engine reports the rejected device names and Vulkan versions.

Every window renders with the forward renderer when its renderer id is empty or `"forward"`; `"none"` opts out. Any other id makes `Engine::init()` return `Error::invalid_argument`. Each rendered window has its own swapchain, frame resources and post-processing chain on one shared device. The first surface selects the device; further surfaces must support presentation from its graphics queue or renderer initialization returns `Error::platform_error` with a diagnostic. Each window resizes independently. A close request releases its target after the GPU is idle; the engine still stops when any window requests closure.

Within the allocated texture descriptor range, runtime scene updates and shadow-array growth retire replaced GPU resources by graphics submission serial instead of waiting for device idle. Each window updates its material and dynamic-vertex copies and changed texture/shadow descriptors only after the matching frame fence completes. `RenderRuntimeUpdateTests` covers edits, texture replacement and shadow growth across two windows, including deferred reclamation and unchanged `deviceWaitIdleCount()`. One-time mesh/texture uploads and shadow-image initialization still wait for their upload fence.

Each recorded window frame resolves visible instances into one draw list sorted by mesh. Camera, spot, point-face and directional-cascade passes conservatively cull world AABBs against their own frusta; touching and invalid bounds remain drawable. `VulkanMesh` computes its culling box from vertices at upload and refreshes it on vertex edits; CPU render meshes store no duplicate box. Vertex/index buffers bind only when the mesh changes, and all shadow layers share one pipeline/descriptor bind. `lastFrameDrawStats()` reports forward draws, shadow draws, vertex-buffer binds and pipeline binds for that window frame, excluding GUI and post-processing.

Static vertex and index buffers use device-local memory, copied through staging buffers retained until the upload fence completes. Uniforms, per-slot materials, dynamic vertices and staging use coherent mapped memory with VMA's sequential-write policy; readbacks use its random-access policy. `VulkanMemoryPolicyTests` checks these policies without a GPU, and `SimpleForwardRendererTests` checks actual mesh and uniform allocation properties.

Material textures generate a full mip chain with linear blits when the format supports linear filtering and blit source/destination usage; otherwise they retain one level. All material textures share one trilinear repeat sampler, with anisotropy enabled only when supported. New textures in one scene sync share one upload submission and fence wait, retaining their staging buffers until completion without a device-idle wait. `textureUploadSubmitCount()` counts these batches; texture tests check batching, mip counts, filtered mip pixels and the format-capability fallback.

Renderer creation selects the material texture ceiling from the physical device's per-stage and pipeline-layout sampler/image limits and its total per-stage resource limit. It reserves room for three shadow descriptors, the uniform and material buffers, and one color attachment. Descriptor slots and GPU texture owners start at the scene's texture count rounded to the next power of two (minimum one), capped at the device ceiling. The embedded fragment shader specializes its texture-array size to that allocation. Adding textures beyond the allocated range grows it geometrically, with one device-idle wait to rebuild the shared pipelines and every window's descriptor sets; ordinary texture updates retain the existing layout. A scene that exceeds the device ceiling returns `Error::capacity_exceeded`; unused descriptor elements use the white fallback texture.

`AssetSystem::sceneTextureCount(scene)` counts distinct texture source paths across the scene's materials, including embedded image keys. Shared paths count once across materials and texture semantics. Scene node queries follow the asset tree's creation (pre-order) order. Node materials are derived from attached meshes in mesh order, and vertex/index counts from the stored arrays; imported mesh bounds remain available through `meshBounds()`. Asset queries keep their owning by-value results. Internally, scene counts and roots read catalog fields without copying, and mesh import consumes borrowed spans directly; simple-engine `mesh*View()` spans must be discarded before catalog mutation or destruction. Render materials refer to the renderer's texture table by index; the table retains each source's canonical path and colour space for reuse and removal.

Primitive planes and cuboids share meshes with identical local extents and UV scale; all procedural objects share materials with the same base colour and texture slot. Place repeated objects through `Transform`. `RenderSystem::addTexturedPlane(half_extent, texture_path, uv_scale, transform)` creates one XZ quad whose UVs span zero to `uv_scale` (default `{1, 1}`). The material sampler repeats outside `[0, 1]`; the game uses `{10, 10}` across its 40 × 40 metre grass field. Removing the last object using a primitive mesh or material releases that resource and any unused textures without a purge. Imported meshes and materials remain cached, including one shared fallback for meshes without a material. Texture decode failures are cached by source and colour space until `clearScene()`; retry a repaired file after clearing the scene. `RenderSystem::sceneTextureCount()` reports live render texture slots.

`removeSceneInstance(instance)` removes one placement; `RenderSystem::removeScene(scene)` releases unused render caches while keeping the source asset catalog available for another placement. `AssetSystem::unloadScene(scene)` also removes that source scene and its exclusively owned nodes, meshes, materials, lights and cameras. Remove every live instance first: unloading otherwise returns `Error::invalid_argument` without changing the scene. Successful unloading invalidates those asset handles; unknown or already unloaded scenes return `Error::missing_object`. Resources used by other scenes or objects remain alive, and submitted GPU resources retire through their normal frame fences.

`RenderSystem::instantiateScene(scene)` creates loose render objects: each object receives its composed world transform once, and only `objectSourceNode()` links it back to its asset node. The render system also keeps one world scene tree whose root node, `sceneRoot()`, always exists and never moves. Pass `SceneInstantiationOptions{.keep_hierarchy = true}` to copy the model's node tree into it below a new instance root (`sceneInstanceRoot(instance)`); `sceneInstanceNode(instance, asset_node)` returns the copy of one asset node, and loading the model again gives another independent copy. `setNodeTransform(node, local)` sets a node's transform relative to its parent and at once moves every object below it, including through nodes without meshes. `createNode`, `setNodeParent` (loops are rejected), `nodeParent`, `nodeChildren` and `removeNode` build and change the tree; hanging a copy's instance root under another node, for example a trailer, makes the whole model follow it. `setObjectParent(object, node)` hangs any render object, including primitives, on a node with its current transform as offset. `setSceneInstanceNodeTransform(instance, node, local)` and `sceneInstanceNodeTransform` still address the copied nodes by asset node. A world placement made with `setObjectTransform` holds until its node or an ancestor is set again. The `hierarchy_car` example drives a car whose body, cabin and wheels follow its root node, with steered front mounts and spinning wheels.

Reparenting keeps a copied node's original instance ownership. Removing that instance removes every node it created, including interior subtrees moved outside its instance root. Child subtrees owned by another instance or created independently move to `sceneRoot()` with their local transforms unchanged. Unrelated objects attached directly to a removed node detach while keeping their world placement. Explicit `removeNode(node)` still removes the entire selected subtree and its attached objects.

`RenderSystem::setCamera(Camera)` sets the default view for every window without an override. After engine initialization, `setCamera(WindowHandle, Camera)` selects an independent view for one window, including before its first rendered frame; `clearCamera(WindowHandle)` restores its use of the current default. Both window-specific calls return `Error::invalid_handle` for unknown, closed or opted-out windows. Each window's drawable extent supplies its aspect ratio for camera uniforms and directional shadow cascades. The selected camera's `clip` supplies both near and far planes (default 0.1 and 100); its position and forward direction define the view without a separate stored view matrix. Camera calls no longer take a `PixelExtent`, and window views and frame snapshots no longer carry ECS camera bindings.

Dear ImGui initializes with the renderer on the first rendered frame and remains bound to the first rendered window. GUI preparation runs before command recording, and its GUI pass runs only there when draw data contains vertices; empty callbacks need no GUI attachment pass. Events from all windows still reach input handling. If SDL3 setup, Vulkan init info, Vulkan backend setup or font upload fails, the engine logs one `[vve::simple] GUI disabled: <stage>` line, releases initialized GUI resources and continues rendering without the GUI.

When `VK_KHR_swapchain_mutable_format` is available, Dear ImGui uses a UNORM swapchain view so authored GUI colours are not sRGB-encoded twice; scene colour and post-processing output stay sRGB. Without the extension, the engine linearizes the initial ImGui theme once. That fallback corrects theme colours, but cannot correct custom draw colours or later theme changes. `GuiColorCaptureTests` checks the UNORM path through a hidden-window PNG capture, including swapchain recreation.

If a GUI callback throws `std::exception`, the engine logs it once, recovers unfinished ImGui windows and stacks, and finishes the frame. The next `Engine::step()` returns `Error::platform_error` once; replacing the throwing callback lets subsequent steps continue.

`RenderSystem::setPostProcessSetup` runs its callback once per rendered window when that window's target is created. Each `vvppl::PostProcessing` reference and its settings references stay valid until that window closes or the engine shuts down, including across resizes, so applications may keep and change them at any time. The `postprocessing` example uses them for runtime sliders and effect checkboxes.

Fallible facade calls return `std::expected<T, Error>`, and `errorName()` names the errors the engine produces. Unresolved texture paths return `io_error`; failed Assimp imports return `asset_import_failed`. Reinitializing an initialized window system returns `already_initialized`.

`EngineBuilder` accepts move-only user systems, including systems without a default constructor, through an explicit `UserSystems` bundle. Building a temporary builder, including after fluent calls, transfers its systems into the engine. For a named builder, use `std::move(builder).build()` to consume it; its systems are moved-from afterward. Calling `build()` on an lvalue copies the configuration and remains available when all systems are copyable.

### Windows build and launch

From an ordinary PowerShell window in the repository, run:

```powershell
.\build_windows.cmd debug
```

Install Visual Studio with the C++ workload and C++ CMake tools, the Vulkan SDK, and vcpkg first. The script discovers the Visual Studio tools, finds vcpkg through `VCPKG_ROOT`, `PATH`, or `C:\vcpkg`, and runs `vcpkg install --triplet x64-windows` on every build so changed manifests are applied. It then configures, compiles, and runs the tests. Repeat the same command to rebuild and retest; no separate CTest path setup is needed. PowerShell requires the leading `.\` for scripts in the current directory.

On Windows, the renderer handles the AMD switchable-graphics workaround at run time. If Vulkan instance creation or physical-device selection fails and `VK_LAYER_AMD_switchable_graphics` is installed but not already listed in `VK_LOADER_LAYERS_DISABLE`, it appends that layer to the process's existing filters, releases the failed initialization resources, logs one compatibility message, and retries once. This also works when `bin\debug\exe\game.exe` is launched from Explorer; it needs no build-machine probe or rebuild after a driver change.

Set `VVE_DISABLE_AMD_SWITCHABLE_GRAPHICS=1` in the application's environment to apply the filter before its first Vulkan instance is created (in PowerShell: `$env:VVE_DISABLE_AMD_SWITCHABLE_GRAPHICS = '1'`). Only this opt-in helps if the layer crashes the process instead of returning an error. Existing layer filters are preserved in both paths.

Use `release` instead of `debug` for a release build (`release` is also the default without an argument), `--no-tests` to omit tests, `--docs` to also generate the Doxygen documentation, or `--clean` to recreate that variant's build directory. The script uses the Ninja generator with MSVC and the build directory `build\<variant>-windows`. Prerequisite, dependency, configure, compile, and test failures are identified separately.

CMake supports `import std` only with Ninja generators, not with the Visual Studio generators. The `debug-windows` and `release-windows` presets therefore also use Ninja with MSVC and the same build directories as the script. They need the compiler environment of Visual Studio: run them from a Developer PowerShell for Visual Studio (or open the folder in Visual Studio, which sets that environment up), not from an ordinary PowerShell window.

For ICODA analysis, build a separate Clang version after installing the dependencies above:

```powershell
.\build_windows_clang.cmd
```

This requires Visual Studio's LLVM and CMake components. It configures the `debug-clang` Ninja preset,
builds the engine, examples and Microsoft's standard-library modules with Clang, and runs CTest.
The compilation database is `build/debug-clang/compile_commands.json`; reload VVE in ICODA after the build.
Use the libclang shipped with that same LLVM installation. Extra CMake options can be passed to the script.

All engine math should go through the exported `vve::math` abstraction layer instead of using raw `glm` types directly. The CMake option `VVE_MATH_USE_DOUBLE` (default `OFF`) selects the scalar type of that layer: `float` with `OFF`, `double` with `ON`. The simple engine currently requires `OFF`: its GPU mirror structs (`RenderVertex`, `FrameUniforms`, `GpuMaterial`) must match the `float` shader layouts, and `static_assert`s stop a build with `VVE_MATH_USE_DOUBLE=ON`.

### Vulkan ICD Selection

The engine links against the Vulkan loader, not directly against individual drivers, and the loader chooses the driver (ICD). On macOS, CMake writes ICD manifests for the drivers shipped with the Vulkan SDK into the build directory: `<build>/vulkan/icd.d/libkosmickrisp_icd.json` for KosmicKrisp and `<build>/vulkan/icd.d/MoltenVK_icd.json` for MoltenVK. To run on KosmicKrisp, point the loader at its manifest when launching:

```bash
VK_ICD_FILENAMES=build/debug-macos/vulkan/icd.d/libkosmickrisp_icd.json bin/debug/exe/testscene
```

The VS Code macOS launch entries and `tools/vscode/run-ctest.sh` set `VK_ICD_FILENAMES` this way, and CTest runs `PostProcessingSmokeTests` with `VK_DRIVER_FILES` pointing to the KosmicKrisp manifest. The engine itself does not choose an ICD at run time. The cache variable `VVE_DEFAULT_VULKAN_ICD` (`system`, `moltenvk`, or `kosmickrisp`; the macOS presets and `build_macos.sh` set `kosmickrisp`) and the manifest paths are passed to the engine library as compile definitions, but the simple engine does not read them. There are no `VVE_VULKAN_ICD` or `VVE_KOSMICKRISP_ICD` environment variables. On macOS, SDL is told to load the Vulkan loader library that CMake found (`SDL_HINT_VULKAN_LIBRARY`).

`vve::Vector<T>` is an alias of `std::vector<T>`, with contiguous storage and `data()`. It follows standard vector invalidation rules: growth may invalidate element pointers, references and iterators. Reserve before borrowing elements, or look them up again after growth.

The public facade is the C++ module `VVEngine` in namespace `vve` (sources in `src/`). It is bound to exactly one engine implementation, selected by the CMake cache variable:

```text
VVE_ENGINE_IMPLEMENTATION_NAMESPACE
```

CMake compiles `src/implementations/<name>.ixx`, the only facade file that names the implementation, together with `src/versions/<name>/`. The facade keeps user code in namespace `vve`; implementation-specific code lives below the selected engine namespace (`vve::simple`).

The active educational implementation is `simple`, and it is the only one;
`v3`, `v4`, and `v5` are retired and are not built:

```powershell
# in a Developer PowerShell for Visual Studio
cmake --preset debug-windows -DVVE_ENGINE_IMPLEMENTATION_NAMESPACE=simple
cmake --build --preset build-debug-windows
```

All example targets follow that single engine namespace selection automatically. The examples live in one folder each below `examples/`: `game`, `testscene`, `sponza`, `light_shadow_debug`, `simple_forward_demo`, and `postprocessing`.

Link applications to `ViennaVulkanEngine::ViennaVulkanEngine`; it supplies the ImGui and SDL3 headers and compile settings. The engine owns their runtime linkage. Adding their static archives directly to an executable can create separate ImGui contexts and SDL window registries beside the shared engine. Windows static-engine builds propagate these dependencies through CMake automatically.

All six examples import `VVE.ExampleSupport` from `examples/common/ExampleSupport.ixx` and link `vve_example_support`. Its `assetRoot(argv0)` searches the current working directory, then the executable's ancestors for an `assets` directory, falling back to the source root configured through `VVE_ASSET_ROOT`. Its `frameLimit(argc, argv)` reads `--frames`; the default is 0, meaning run until the window closes. CTest always passes an explicit frame limit.

The presets and build scripts are host-aware:
- Windows uses the `x64-windows` vcpkg triplet
- Linux uses the repository's `x64-linux-llvm` overlay triplet so dependencies share the engine's Clang/libc++ ABI. The Linux presets and standalone dependency toolchain default to LLVM 18. `build_linux.sh` uses `VVE_LLVM_VERSION` when set (for example `VVE_LLVM_VERSION=18 ./build_linux.sh debug`), otherwise the newest version with `clang-NN`, `clang++-NN`, `clang-scan-deps-NN` on PATH and `/usr/lib/llvm-NN/lib/libc++.modules.json`. It exports that version for vcpkg, uses CMake from `CMAKE` or PATH and the adjacent CTest (or CTest from PATH), and reuses the build tree unless the compiler changes or `--clean` is requested.
- macOS uses the `arm64-osx` vcpkg triplet

`build_windows.cmd` and `build_macos.sh` run `vcpkg install` automatically. `build_linux.sh` also runs it when vcpkg is available through `VCPKG_ROOT` or PATH, otherwise it skips installation and uses existing packages. The CMake presets require an explicit dependency installation. CMake consumes the installed packages from `vcpkg_installed/<triplet>`; `vcpkg-configuration.json` registers the Linux overlay triplet.

Before the first build, run:

```powershell
.\build_windows.cmd debug  # Windows: install dependencies, configure, build, test

# or

./build_linux.sh debug         # Linux: install dependencies if vcpkg is found, configure, build, test
# (or: vcpkg install --triplet x64-linux-llvm; cmake --preset debug-linux; cmake --build --preset build-debug-linux)

# or

./build_macos.sh debug         # macOS: install dependencies, configure, build, test
# (or: vcpkg install --triplet arm64-osx; cmake --preset debug-macos; cmake --build --preset build-debug-macos)
```

`build_linux.sh` and `build_macos.sh` accept `debug` or `release` (the default) and `--clean`. They use the build directories `build/<variant>-linux` and `build/macos-<variant>`. The presets use `build/<preset name>`.

A parent project using `add_subdirectory(ViennaVulkanEngine)` must set `CMAKE_EXPERIMENTAL_CXX_IMPORT_STD` to the gate UUID for its CMake version and `CMAKE_CXX_MODULE_STD` to `ON`, using normal variables before its own `project()`; VVE's settings stay in its directory scope.

Release builds use matching `release-*` presets, for example:

```bash
cmake --preset release-macos-arm64-llvm
cmake --build --preset build-release-macos-arm64-llvm
```

Executables are written to the project root `bin` directory, below a path that uses only the build variant, for example `bin/debug/exe/testscene` or `bin/release/exe/testscene`. Shared libraries (Linux, macOS) are built below the selected build directory and mirrored to `bin/<variant>/lib`. Platform names such as `Mac`, `Windows`, or `Linux` are not used below `bin`. The `light_shadow_debug` example writes its verification text and PNG to `bin/<variant>/verify`.

VS Code is configured to use CMake Tools variants instead of presets so the `CMake: Select Variant` command offers `Debug` and `Release`. The VS Code variant builds use `build/vscode-debug` and `build/vscode-release` and always the Ninja generator (needed for `import std`). The vcpkg triplet comes from the selected kit, or else from the host default in CMakeLists.txt (`x64-windows`, `x64-linux-llvm`, `arm64-osx`). Select these kits: `Homebrew LLVM arm64` on Apple Silicon macOS, `LLVM 18 libc++ (Linux)` on Linux (both defined in `.vscode/cmake-kits.json`), and a Visual Studio `amd64` kit on Windows.

The VS Code Run and Debug list contains `Windows Debug (choose executable)`, `game`, `testscene`, `postprocessing`, `sponza`, `world tests`, and `all tests`. Except for the first one, each launch asks for `Platform` (`Mac`, `Windows`, `Linux`) and `Variant` (`debug`, `release`) and then runs the matching build task before launch. The Windows-only first entry builds the Windows debug variant and asks which executable to debug. Select the platform that matches the machine running VS Code; these launch options are shared across operating systems, not cross-compilers. On macOS the launch entries set `VK_ICD_FILENAMES` to the KosmicKrisp manifest in `build/vscode-<variant>`.

If CMake Tools asks for a kit on Apple Silicon macOS, select `Homebrew LLVM arm64`. The workspace also uses `cmake/toolchains/macos-arm64-homebrew-llvm.cmake` so stale AppleClang kit selections are redirected to the Homebrew LLVM compiler required for `import std`.

On Apple Silicon macOS with Homebrew LLVM, use the arm64 LLVM preset (it loads the vcpkg toolchain from `$VCPKG_ROOT`, so set that variable first):

```bash
brew install ninja llvm
vcpkg install --triplet arm64-osx
cmake --preset debug-macos-arm64-llvm
cmake --build --preset build-debug-macos-arm64-llvm
```

## Installing vcpkg

vcpkg must be installed and available in your PATH before building.

### macOS

**Option 1: Homebrew (recommended)**
```bash
brew install vcpkg
```

**Option 2: Clone and bootstrap**
```bash
git clone https://github.com/Microsoft/vcpkg.git ~/vcpkg
cd ~/vcpkg
./bootstrap-vcpkg.sh
# Add to PATH: export PATH="$HOME/vcpkg:$PATH"
```

**Verify installation:**
```bash
vcpkg version
```

### Linux

```bash
git clone https://github.com/Microsoft/vcpkg.git ~/vcpkg
cd ~/vcpkg
./bootstrap-vcpkg.sh
# Add to PATH: export PATH="$HOME/vcpkg:$PATH"
```

### Windows

Download the latest release from https://github.com/microsoft/vcpkg or clone and bootstrap:
```powershell
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.bat
```

## VS Code

The launch entries run these task labels internally: `Build Mac debug`, `Build Mac release`, `Build Windows debug`, `Build Windows release`, `Build Linux debug`, and `Build Linux release`. To run tests in VS Code, use the `world tests` or `all tests` launch entry and choose the desired platform and variant.

## Unit Tests

Shared test helpers use `import VVE.TestSupport` and the `vve_test_support` library, defined in `tests/CMakeLists.txt` from `tests/support/TestSupport.ixx`. The `vve::test` helpers configure hidden engines, write text fixtures in each test's build directory, and measure RGBA captures. Scene data, masks, tolerances and assertions remain in the tests. Sponza captures use `VVE_TEST_SPONZA_SCENE`, the configured source path, without runtime asset discovery.

The build scripts run all tests after a successful build (`build_windows.cmd` skips them with `--no-tests`). To build and run all unit tests from the project root:

```powershell
.\build_windows.cmd debug
```

To rerun the tests of an existing build, or to list the registered tests without running them:

```powershell
ctest --test-dir build/debug-windows --output-on-failure
ctest --test-dir build/debug-windows -N
```

For other build directories (`build/debug-linux`, `build/macos-debug`, ...) replace the `--test-dir` argument; add `-R <name>` to run selected tests.

Tests that need a window or Vulkan device carry the `gpu` label. Use `ctest --test-dir build/debug-linux -LE gpu -N` to list the CPU subset, and omit `-N` to run it without a display or Vulkan device. Register window/GPU tests with `vve_add_engine_test(<Name> GPU [extra libraries])`. All six example tests also carry the `example` label, selectable with `-L example`. The four Ring Racer game runs take minutes and carry the `racer` label; the build scripts skip them with `-LE racer`, and `ctest --test-dir <build> -L racer` runs them.

`VulkanDeviceRequirementsTests` checks the Vulkan 1.3 version and feature requirements using synthetic capabilities. It belongs to the CPU subset and creates no Vulkan instance, device or window.

`RenderFrameCountTests` checks that an uninitialized render system returns `Error::not_initialized`, then uses a hidden 64×64 window to verify presentation and skipped-frame accounting. `renderedFrameCount()` and rendering FPS advance once when at least one window presents; `lastRenderedWindowCount()` counts the windows actually presented, and is zero when all targets skip. `MultiWindowRenderTests` covers two independently sized windows, renderer opt-out, GUI ownership, post-processing chains, resize and teardown with validation. `captureFrameToPng(WindowHandle, path)` captures one rendered window; `captureFrameToPng(path)` uses the first remaining rendered window.

The simple renderer bounds its in-flight fence wait to 1 second and image acquisition to 100 ms. Fence timeouts and unavailable swapchain images skip the frame silently and increment the implementation diagnostic `ForwardRenderer::skippedFrameCount()`; they do not count as presented frames. Presented frames, temporary skips and fatal failures have distinct internal results. Recoverable swapchain changes remain nonfatal; fatal Vulkan failures reach the public `Engine::step()` and `run()` results with stage and `VkResult` diagnostics retained in the log. A fatal renderer failure stays failed until renderer cleanup. `RenderAcquireActionTests` checks the acquire-result policy without a window or device. Presentation (`vkQueuePresentKHR`) can still block; these limits apply only to the fence and acquire waits.

With `BUILD_TESTING` and the simple engine, `vve_add_example_test(<target>)` in `examples/CMakeLists.txt` registers `game`, `testscene`, `sponza` and `simple_forward_demo` with `--frames 3` and a 60-second timeout. These tests run from the repository root. Run all example tests with `ctest --test-dir build/debug-linux -L example --output-on-failure`. The `simple_forward_demo` test passes `--output <build>/examples/simple_forward_demo/verify/simple_forward_demo_capture.png`; without `--output`, the example keeps its executable-relative capture path.

Every test is a C++ executable built from one file in `tests/` and registered in [tests/CMakeLists.txt](tests/CMakeLists.txt) with `vve_add_engine_test`. CTest additionally runs `LightShadowDebugExample` (the `light_shadow_debug` example, which writes `bin/<variant>/verify/light_shadow_debug.txt` and `.png`) and `PostProcessingSmokeTests` (`postprocessing --frames 3 --all-effects`, which compiles and runs all 15 effects and fails on a non-zero exit code). Without `--all-effects`, `postprocessing` keeps its default chromatic, vignette, tonemap, greyscale and film grain chain. The rendering tests `SimpleForwardRendererTests`, `RenderTextureDedupTests`, `RenderMaterialImportTests`, `RenderLightingCaptureTests`, `RenderSponzaCaptureTests`, and `RenderMeshDedupTests` open hidden SDL windows (64 to 256 pixels) and need a Vulkan device that can present to them. Tests that pass `visible(false)` window setups also open hidden windows. They use the platform video driver; on a Linux machine without a display (`DISPLAY` and `WAYLAND_DISPLAY` unset), `build_linux.sh` switches to SDL's offscreen driver, which needs a Vulkan driver with `VK_EXT_headless_surface`.

## Ring Racer

`examples/racer` is an intermediate game on the facade: lap racing against three computer cars, a time trial against your ghost, night mode with headlights and a photographed sky, a countryside with grandstand, paddock, town, farm and fields, and engine, tyre, crash and crowd sounds. It shows user systems, the ECS, box colliders in game code, procedural meshes, pitched audio and most render features. When examples are enabled, CMake downloads Kenney's Racing and City Builder starter kits into the ignored `assets/racer/kenney` folders during configuration. See [examples/racer/README.md](examples/racer/README.md).

## Island Air

`examples/island_delivery` is a Full HD cargo-flight game across 256 islands using the VVE facade and ECS.
Choose cargo destinations on a map, fly a textured twin-prop aircraft, land and stop to unload, then collect the next job.
Regional navigation shows the direction to fly and runway orientation; nose-wheel steering allows runway turnarounds.
It includes bounded scenery streaming, chase/pilot-eye cameras, PBR scenery, animated water and recorded positional aircraft audio.
CMake downloads hash-pinned external assets into the build directory; the repository contains no game asset binaries.
See [examples/island_delivery/README.md](examples/island_delivery/README.md) for controls, build options and deterministic tests.

## Rendering features

The `vve::RenderSystem` facade offers a few features a simple game needs:

- Transparency: `setObjectOpacity(object, opacity)` (0 to 1, read back with `objectOpacity`) blends an object over what lies behind it. Imported glTF materials keep their alpha mode: `OPAQUE` ignores alpha, `MASK` cuts holes where the base-colour alpha is below 0.5, and `BLEND` is see-through; OBJ materials with a dissolve below 1 are blended. Transparent objects are drawn after solid ones, far to near, and cast no shadows; cut-out objects cast solid shadows.
- Background: `setBackgroundColor(color)` sets the colour every window clears to (default light blue); `backgroundColor()` reads it.
- Skybox: `setSkybox(panorama)` surrounds every camera with a 2:1 equirectangular image (top row straight up); `clearSkybox()` and `clearScene()` remove it. A missing image returns `Error::io_error`.
- Material lighting: `setMaterialLighting(MaterialLighting::physically_based)` enables GGX direct lighting and approximate illumination/reflections from the skybox. Classic lighting remains the default. `setObjectSurface(object, SurfaceMaterial{.roughness = 0.3F, .metalness = 0.0F})` changes one object's factors without changing shared materials; imported maps still apply. Set colour/texture before surface factors, since those look setters reset the other material properties. Panorama filtering uses eight fixed samples and ordinary mipmaps; it is an inexpensive approximation without local reflections or precomputed irradiance maps.
- Haze: `setAtmosphere(AtmosphereSettings{.density = 0.002F, .height_falloff = 0.01F})` blends surfaces towards the chosen haze colour with distance and decreasing density above `base_height`. Zero density (the default) disables it. This is analytic height fog, with no volumetric clouds or light scattering; the skybox remains unchanged.
- Water: `addOcean(OceanDescriptor{})` creates a finite 64×64-cell horizontal patch with two analytic waves and reflective lighting. It returns an ordinary object handle, so normal transform, visibility, opacity and removal calls apply; bounds conservatively include wave height. Drive it with `setAnimationTime(AnimationTime{elapsed_seconds})`; the default clock is paused at zero. The patch has no refraction, foam or shoreline simulation.
- Antialiasing: call `setAntialiasing(Antialiasing::msaa4)` before the first rendered frame for four-sample geometry coverage. The default is `none`; changing the mode after renderer initialization returns `Error::already_initialized`. Unsupported devices report `Error::platform_error` when rendering initializes. Post-processing, frame capture and GUI consume the resolved single-sample image; shadow maps remain single-sample.
- Sprites: `addSprite(size, texture, color, transform)` adds an unlit textured quad that always faces each window's camera; its texture alpha makes it see-through. Only translation and x/y scale apply, and sprites cast no shadows. Particles are many sprites moved by game code.
- Changing looks: `setObjectColor(object, color)` keeps the object's texture and `setObjectTexture(object, path)` keeps its colour. An imported object becomes a plain coloured object; its other PBR values are not kept.
- Shapes and meshes: `addSphere(radius, color, transform)` adds a sphere (equal radii share one mesh). `addMesh(MeshDescriptor)` adds geometry built by game code: positions and triangle indices, optional normals (empty gives flat face normals), UVs (empty gives zero), colour, base-colour texture and transform; mismatched sizes return `Error::invalid_argument`.
- Picking: `objectBounds(object)` returns an object's world box. `raycast(origin, direction, max_distance)` returns the nearest visible object whose box the ray enters, as `RaycastHit{object, distance}`; it tests boxes, not triangles. `screenRay(window, point)` returns the origin and direction of the ray through a window point ((0,0) top left, (1,1) bottom right, like `mousePosition`) from that window's camera.

`RenderMaterialLightingTests`, `RenderEnvironmentTests` and `RenderAntialiasingTests`
check these extensions with deterministic pixel comparisons, saved PNGs and
Vulkan validation. Captures are in `build/<variant>-linux/tests/tmp/<test-name>`
on Linux (the corresponding build directory on other platforms).

Input (`vve::InputState`, `vve::WindowSystem`): `vve::Key` names letters, arrows, Shift, Ctrl, Alt, Space, Enter, Tab, Backspace, Delete, Insert, Home, End, Page Up/Down and F1 to F12; `vve::key('w')` names any letter (either case), digit or punctuation key. The mouse buttons `Key::mouse_left`, `mouse_middle` and `mouse_right` work with `isKeyDown`, `wasKeyPressed` and `wasKeyReleased` like keys. `setMouseCaptured(window, true)` hides the pointer and keeps `mouseDelta` coming at the window border, for mouse look.

Fullscreen: `WindowSetup{}.fullscreen(true)` starts a window in fullscreen at the desktop resolution, and `WindowSystem::setFullscreen(window, on)` switches it later; the renderer follows the new size.

HUD images: `GuiSystem::loadImage(path)` loads an image for the GUI and returns a `GuiImageHandle`; inside a `draw` callback, `imageId(handle)` gives the `ImTextureID` for `ImGui::Image` (0 until the GUI runs and the image is on the GPU, so skip drawing then). Images stay loaded for the engine's lifetime, also across `clearScene`; their bytes are shown unchanged, like ImGui colours. The ImGui descriptor pool holds 16 textures, one of them the font.

## Audio

The engine's `vve::AudioSystem` facade uses SDL3_mixer, installed by the normal
vcpkg manifest build. Obtain it with `world.get<vve::AudioSystem>()`, call
`audio.init()` explicitly, then load a sound and play independent overlapping
voices. Looping, pause/resume, voice and master volume, and resource cleanup are
available without including SDL headers in your game.
`setListener(AudioListener)` and `setSource(voice, AudioSource)` add directional
speaker mixing, clamped inverse-distance attenuation and Doppler; positions use
metres and velocities metres per second. `clearSource(voice)` restores ordinary
2D playback. Listener and source updates are explicit and independent of windows.

The V3 Crate Collector also includes V2's original Dance, Ophelia and Never Get
Out soundtrack. Music starts automatically; choose a track, adjust volume or
pause music in the score overlay. Its music and original attribution are staged
in `audio/v2` beside `game`. The vcpkg manifest enables the MP3 decoder.

Run `bin/<variant>/exe/audio_demo` (`.exe` on Windows) for a complete example.
Its original WAV is copied beside the executable; no launch script is required.
See [the audio guide](docs/Audio.md) for the API, error handling and linking from
your own project. Audio tests use a dummy device during CTest.

## Doxygen

If Doxygen is installed, CMake adds a `docs` target. On Windows, generate the documentation from the project root with `build_docs.cmd`, which runs:

```powershell
.\build_windows.cmd debug --no-tests --docs
```

On Linux and macOS, build the target in an existing build directory, for example `cmake --build build/debug-linux --target docs`.

The generated output is written to [docs/build](docs/build). The HTML entry page is usually [docs/build/html/index.html](docs/build/html/index.html).
