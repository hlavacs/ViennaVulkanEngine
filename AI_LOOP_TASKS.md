# AI loop tasks — review follow-up

Work list for the AI loop, created on 27 September 2026 against branch `V3` at commit `8185b02`. It continues the code review of 27 September 2026. That review's crash, rendering, bookkeeping, input, dead-code and stale-comment findings are fixed in commit `0a7fa80` and are not repeated here. Every task below was checked against `8185b02`; line numbers refer to that commit and drift as tasks land, so search for the named symbols.

## How to work

- Work through the phases in order. Inside a phase, keep the listed order unless a task says otherwise. Each task names its dependencies under **Risk / notes**.
- One task per commit. Commit message: `<ID>: <title>`, for example `B1: Let the post-processing smoke test fail on a bad exit code`.
- Before every commit, build and run all tests with the script for your platform: `./build_linux.sh debug`, `./build_macos.sh debug` or `.\build_windows.cmd debug`. All three platforms must stay green, so guard platform-specific code and do not assume one compiler.
- Every behaviour change needs a test that fails before the change and passes after it. Do not weaken or delete existing checks to make a task pass. Change a test only where the task says it encodes the old behaviour.
- Follow `AGENTS.md` and `CLAUDE.md`:
  - the facade in `src/*.ixx` stays implementation-free, and the simple engine lives in `src/versions/simple` (`vve::simple`);
  - errors are returned as `std::expected<T, Error>`;
  - make surgical changes and match the existing style;
  - keep each file's line endings.
- The maintainer's decisions are recorded in the table below; tasks marked **Decision DEC-n** follow the chosen option. Where a task text still mentions the option that was not chosen, the table wins.
- Update the documentation in the same commit when behaviour or API changes: `README.md`, `AGENTS.md`, `src/versions/simple/AGENTS.md`, `docs/notes/architecture.md`, `examples/FACADE_AUDIT.md`.
- Remove everything that becomes unnecessary: when a task leaves code, a test helper, a file or a documentation passage unused, delete it in the same commit.
- If a task turns out to be wrong, already done or much larger than described, do not force it. Add one line to the loop log at the end of this file and continue with the next task.
- Never edit `vcpkg_installed/`, `build/` or `bin/`.

## Decisions of the maintainer

Decided on 27 September 2026. Overall rule: **remove everything that is unnecessary.**

| ID | Question | Decision | Tasks |
|---|---|---|---|
| DEC-1 | Minimum Vulkan version | Require Vulkan 1.3; reject older devices with a clear message. | R12 |
| DEC-2 | Several windows | Real multi-window rendering: every window renders, each with its own swapchain and camera. | W3a, W3b, W3c, W12, D11 |
| DEC-3 | Mouse units | Window coordinates normalised to 0..1 (origin top left, y down), independent of window size and pixel density. `SDL_WINDOW_HIGH_PIXEL_DENSITY` stays off. | W7 |
| DEC-4 | Public API removals | Remove write-only and never-used API: the camera plumbing, the `PixelExtent` parameter of `setCamera`, the ECS-based window-camera API (replaced by per-window cameras in W3c), and the `Error` values nothing produces. Apply `ClipPlanes`, default far plane 100. | D11, W3c, E1 |
| DEC-5 | `vve::Vector` | Remove the segmented vector; `vve::Vector<T>` becomes an alias of `std::vector<T>`. | D12, D13, D14 |
| DEC-6 | Engine pimpl | Replace `EngineState` and its 11 accessors with one `EngineImpl` pointer. | D10 |
| DEC-7 | Build output location | Keep writing executables to `<src>/bin`. B2 is not done. | B2 |
| DEC-8 | Post-processing API | Keep `setPostProcessSetup` and direct vvppl access as they are; the example already changes effects at runtime through the pointers it keeps. `<VVPPL.h>` stays in the facade. | B5 |
| DEC-9 | Image quality (2 November 2026; DEC-IQ-1 / DEC-IQ-2) | MSAA and SMAA are options in VVE's simple renderer, both off by default. SMAA is implemented inside VVE, independent of vvppl; vvppl stays pinned and unchanged and DEC-8 still holds. Khronos PBR Neutral tone mapping is an opt-in VVE option. VVE's default render path and appearance remain unchanged, verified against pre-task captures using the image-quality comparison policy; identical GPU pixels across runs, drivers or platforms are not promised. | IQ-01, IQ-02, IQ-03, IQ-10 |

## Task order

- **Phase 0 — Housekeeping:** H1, W13, B12, B6
- **Phase 1 — Make the tests able to fail (safety net for everything below):** R14, R15, T5, T1, B1, T4, T7, T3, T6, T2, T9, T10
- **Phase 2 — Correctness and multi-window rendering:** R12, W8, W4, B5, W2, W11, W9, W14, W10, R10, R16, W1, W5, W7, R9, W3a, W3b, W3c, W12
- **Phase 3 — GPU cost:** R5, R8, R7, R1, R3, R4, R6, R2, T8
- **Phase 4 — Duplicated data and data model:** D3, D9, D1, E2, D6, D7, D11, D12, D5, D14, D13, D4, D8, D10, W6
- **Phase 5 — Build and infrastructure:** B7, B8, B10, B4, B9, B11
- **Phase 6 — Examples and low priority:** E1, R13

## Phase 0 — Housekeeping

### H1 — Move the stray VS Code files into .vscode and untrack generated leftovers
- **Where:** `Claude outputs/{settings.json,cmake-kits.json,tasks.json,vve-review-fixes-1-36.patch}` (added in afbbc77); `.vscode/{settings,cmake-kits,tasks}.json`; `verify/light_shadow_debug.{png,txt}`; `build_log.txt`; `docs/simple_forward_demo_{render_analysis,run,runtime_diagnostics}.txt`; `imgui.ini`; `.gitignore`
- **Problem:** afbbc77 committed the intended new VS Code files into `Claude outputs/` and left `.vscode/` stale:
  - `settings.json` adds `"cmake.generator": "Ninja"`, which `import std` needs on Windows. It drops `VVE_VCPKG_TRIPLET=x64-windows` (wrong on Linux and macOS; CMakeLists now picks the triplet per host) and `VVE_DEFAULT_VULKAN_ICD=system`.
  - `cmake-kits.json` adds a "LLVM 18 libc++ (Linux)" kit that matches the linux presets.
  - `tasks.json`: the Linux vcpkg task becomes `--triplet x64-linux-llvm --overlay-triplets=triplets`. The macOS configure tasks change `moltenvk`→`kosmickrisp` and `VVE_ENGINE_IMPLEMENTATION_NAMESPACE=v5`→`simple`; today they fail with "Unknown engine implementation 'v5'" (`src/CMakeLists.txt:45-49`). The "Configure Windows debug/release" preset tasks are removed. "Build Windows release" now runs `build_windows.cmd release --no-tests`, like the debug task.
  - All three files are valid JSON. `launch.json`'s `preLaunchTask` names still exist.

  The other files are unreferenced; `git grep` finds no reader of any of them:
  - `verify/` holds an old light_shadow_debug output. It was written cwd-relative through the fallbacks at `light_shadow_debug.cpp:40,45-47`.
  - `build_log.txt` is a Windows build log.
  - `docs/simple_forward_demo_*.txt` are old run outputs. Doxygen reads only `src/` and README.
  - `imgui.ini` is rewritten by every GUI run.
- **Change:**
  - Copy the three JSON files over `.vscode/`, then run `git rm -r "Claude outputs" verify build_log.txt docs/simple_forward_demo_render_analysis.txt docs/simple_forward_demo_run.txt docs/simple_forward_demo_runtime_diagnostics.txt` and `git rm --cached imgui.ini`.
  - Add `/verify/`, `imgui.ini` and `/build_log*.txt` to `.gitignore`.
  - The imgui.ini part is the same as W13's git step; do it once.
- **Done when:**
  - `git ls-files "Claude outputs" verify build_log.txt imgui.ini "docs/simple_forward_demo_*"` is empty.
  - For each file f, `diff <(git show afbbc77:"Claude outputs/f") .vscode/f` is empty.
  - `git grep -n "Claude outputs"` is empty.
  - Running `postprocessing --frames 3` from the repo root leaves `git status` clean.

### W13 — Stop Dear ImGui from writing imgui.ini into the working directory
- **Where:** `src/versions/simple/GUI.ixx:56-58` (`initContext`); tracked `imgui.ini` in the repo root
- **Problem:** `io.IniFilename` keeps ImGui's default "imgui.ini", so every run that initialises the GUI (all GPU tests and examples) writes the window layout into its current directory. Running examples from the repo root dirties the tracked `imgui.ini`, which was committed in 10b6659, bd19fc5 and 22c2c84. Parallel ctest runs also write the same file.
- **Change:** In `GuiSystem::initContext`, set `ImGui::GetIO().IniFilename = nullptr`. Optionally add an opt-in `GuiSystem::setIniFile(std::filesystem::path)` that keeps the string alive. Run `git rm imgui.ini` and add `imgui.ini` to `.gitignore`.
- **Done when:**
  - `tests/GuiSystemTests.cpp` (add `imgui::imgui` to its `vve_add_engine_test`) asserts `ImGui::GetIO().IniFilename == nullptr` after `initContext()`, then calls `shutdownContext()`.
  - After `./build_linux.sh debug`, `find build bin -name imgui.ini` finds nothing.
  - `git status` is clean after running `postprocessing --frames 3` from the repo root.

### B12 — Remove the CTestTestfile overwrite that CMake discards anyway
- **Where:** `CMakeLists.txt:457-462` (`file(WRITE "${CMAKE_BINARY_DIR}/CTestTestfile.cmake" "subdirs(\"tests\")\n")`)
- **Problem:** CMake regenerates `CTestTestfile.cmake` at generate time, after configure. I verified this with a scratch project: the written file is replaced by `subdirs("a")`/`subdirs("tests")`. So the line has no effect. If it ever did take effect, it would silently drop `LightShadowDebugExample`, which is registered in `examples/`.
- **Change:** Delete the `if(PROJECT_IS_TOP_LEVEL) file(WRITE ...)` block.
- **Done when:** `ctest -N` lists the same tests before and after, including `LightShadowDebugExample` and `PostProcessingSmokeTests`.

### B6 — Delete the dead vvppl shader patch
- **Where:** `cmake/PatchVvpplShaders.cmake:1-23`; `CMakeLists.txt:179-181`
- **Problem:** The patch was written for vvppl v0.3. In v1.0 the sample line already comes before the anchor (`shaders/emboss.slang:38` vs `:40`, `sobel.slang:43` vs `:45`), so the rewrite branch never runs. Its only remaining effect is a `FATAL_ERROR` on Apple as soon as either line changes upstream, for example after a re-tag (B4). The patch also edits the fetched sources in place.
- **Change:** Delete the file and the `if(APPLE) include(...)` block.
- **Done when:** `git grep -n PatchVvppl` is empty. On macOS, `PostProcessingSmokeTests` with `--all-effects` (B1), which includes emboss and sobel, passes on KosmicKrisp.

## Phase 1 — Make the tests able to fail (safety net for everything below)

### R14 — Report validation errors (including sync hazards) to the tests
- **Where:** `S/Vulkan/Device.ixx:63-69,81-92`, GPU tests in `tests/`
- **Problem:** Debug builds enable `VK_LAYER_KHRONOS_validation`, but no `VK_EXT_debug_utils` messenger is registered and synchronization validation is off. Validation messages only reach the layer's default log and no ctest can fail on them, so layout and barrier regressions (R1, R3, R5, R8, R9) are not caught automatically.
- **Change:** In `VulkanInstance::create` (Debug, layer present), enable `VK_EXT_debug_utils` if it is listed, chain `VkDebugUtilsMessengerCreateInfoEXT` into the instance `pNext`, create a persistent messenger via `vkGetInstanceProcAddr`, and destroy it in `cleanup()` before the instance. The callback prints to stderr and increments an atomic counter for ERROR severity. Enable sync validation with `VkLayerSettingsCreateInfoEXT` (`validate_sync = true`), falling back to `VkValidationFeaturesEXT` on older SDKs. Expose `ForwardRenderer::validationActive()` and `validationErrorCount()`.
- **Done when:** `SimpleForwardRendererTests`, `RenderLightingCaptureTests`, `RenderTextureDedupTests`, `RenderMeshDedupTests`, `RenderMaterialImportTests` and `RenderSponzaCaptureTests` fail when `validationActive() && validationErrorCount() != 0`. `SimpleForwardRendererTests` also injects one ERROR with `vkSubmitDebugUtilsMessageEXT` and asserts the counter rose by exactly 1, which proves the plumbing.
- **Risk / notes:** This will probably expose existing errors, which must be fixed first. Machines without the layer skip the check. Sync validation slows the GPU tests.

### R15 — Create the swapchain with clipped = VK_FALSE because frames are read back
- **Where:** `S/Vulkan/Presentation.ixx:127`, `S/Render/RendererDebug.cpp:79-90`, `S/Render/RendererDraw.cpp:119-121`
- **Problem:** `clipped = VK_TRUE` allows the implementation to skip pixels of the surface that are not visible and leaves them undefined when read back. The engine reads swapchain images back (`captureFrameToPng`, the in-frame readback), and all capture tests render into hidden windows, i.e. fully invisible surfaces, so on a conforming implementation the captured PNGs may contain undefined data.
- **Change:** Set `.clipped = VK_FALSE` in `VulkanSwapchain::create` with a comment citing the readback. The cost on desktop is negligible.
- **Done when:** `grep -n "clipped" src/versions/simple/Vulkan/Presentation.ixx` shows `VK_FALSE`. `RenderLightingCaptureTests` and `RenderSponzaCaptureTests` pass with hidden windows on Linux, Windows and macOS.

### T5 — Make light_shadow_debug, the Sponza capture and the lighting capture tests able to fail
- **Where:** `examples/light_shadow_debug/CMakeLists.txt:16-19` (`--skip-gpu-asserts`); `examples/light_shadow_debug/light_shadow_debug.cpp:152-217` (exits 0 whatever it writes); `tests/RenderSponzaCaptureTests.cpp:29-42,74-78`; `tests/RenderLightingCaptureTests.cpp:55-60,134-147`
- **Problem:**
  - `--skip-gpu-asserts` is parsed nowhere, and `LightShadowDebugExample` exits 0 even when `has_gpu=0` or `*_shadow_layers_distinct=0`.
  - The Sponza test returns 0 ("skipped") when it cannot find the asset. The asset is tracked in git, so a miss means the exe-relative search failed, for example in an out-of-source build.
  - The lighting test passes when the mean absolute difference over all RGBA bytes is > 0.01. One pixel off by 255 in three channels is enough, while the triangle covers only a small part of the 128×128 image.
- **Change:**
  - light_shadow_debug: drop the flag from CMake. After writing the report, return 6 if any sample has `has_gpu == 0`, if a light type with ≥ 2 samples has non-distinct layers, or if no PNG was written.
  - Sponza: pass `VVE_TEST_SPONZA_SCENE="${PROJECT_SOURCE_DIR}/assets/sea_keep_lonely_watcher/scene.gltf"` as a compile definition, delete `assetRoot`, and return 21 when the file is missing.
  - Lighting: build a mask of the pixels where the unlit capture differs from its pixel (0,0) (the clear colour) by > 8 in any channel, and require at least 200 of them. Compute the statistics only over the mask. Require the lit/unlit and normal/flat mean differences to be at least half of the values the current renderer prints on the reference machine. Also require lit mean > unlit mean × 1.1: the light points at the front face, N·L ≈ 0.9.
- **Done when:**
  - All three pass.
  - Negative checks:
    - `setShadowDepthReadback(false)` in the example gives exit 6.
    - Renaming the Sponza asset gives exit 21.
    - Replacing the normal map in `render_material_import.mtl` with the diffuse map makes the lighting test fail.

### T1 — Run the GPU readback and pass-order checks in Release too
- **Where:** `tests/SimpleForwardRendererTests.cpp:659-661,682-684,686-700,717-722,724-726`
- **Problem:** The GPU shadow-depth readback, the pass-order check (exit 17), the shadow-sample checks (8, 18, 28) and the directional toggle coverage (29) are all inside `#ifndef NDEBUG`, so a Release `ctest` checks no GPU result. Nothing in the engine requires this: `setGpuDebugReadback`, `recordedPassOrder` and the readback are compiled in every configuration. The spot-layer check at :717 only runs `if (spot_samples.size() == active_spot_light_count)`, so a frame without samples silently passes.
- **Change:** Delete all four `#ifndef NDEBUG` guards. Make :717 a hard requirement: return 13 when the sample count differs from `active_spot_light_count`.
- **Done when:** `grep -n NDEBUG tests/SimpleForwardRendererTests.cpp` is empty, and `./build_linux.sh release` (and the Windows release build) pass. Negative check: pushing `RecordedPass::forward_color` before the shadow passes in `RendererDraw.cpp` makes the Release test fail with 17.

### B1 — Let the post-processing smoke test fail on a bad exit code and compile every effect
- **Where:** `tests/CMakeLists.txt:52-67` (`PASS_REGULAR_EXPRESSION` at :60); `examples/postprocessing/postprocessing.cpp:128-168` (the default chain at :134-138)
- **Problem:** With `PASS_REGULAR_EXPRESSION` set, CTest ignores the exit code. I checked this with a scratch project: a test that prints the pattern and exits with 3 is reported as "Passed". So a crash or a non-zero return after `frames=3` is printed goes unnoticed. vvppl creates a pipeline only when its effect is added (`VVPPL.cpp:295-301`), and the example adds only chromatic, vignette, tonemap, greyscale and film grain. The other 10 shaders (including invert) are never compiled for the device, so a broken shader such as the old KosmicKrisp emboss/sobel failure would pass.
- **Change:** Remove `PASS_REGULAR_EXPRESSION` and keep `FAIL_REGULAR_EXPRESSION`. With only a fail pattern, CTest still checks the exit code (verified). Add an `--all-effects` flag to the example that enables all 15 effects in the setup, prints `[postprocessing] effects=15` and renders as usual. Register the test as `postprocessing --frames 3 --all-effects`.
- **Done when:** `ctest -R PostProcessingSmokeTests --output-on-failure` passes and its output contains `effects=15`. `ctest --show-only=json-v1` shows no `PASS_REGULAR_EXPRESSION` for the test. Negative check: change `return 0` at the end of `main` to `return 7` locally and the test fails.
- **Risk / notes:** On macOS the test runs with KosmicKrisp (`:63-66`). All 15 Metal translations must succeed there, so run it on a Mac before merging.

### T4 — Make RenderTextureDedupTests independent of the build drive
- **Where:** `tests/RenderTextureDedupTests.cpp:34-37`
- **Problem:** `std::filesystem::relative(texture_a, current_path())` is empty when the build tree (the test's cwd) and the source tree are on different Windows drives. The test then returns 2 before testing anything.
- **Change:** Set the cwd explicitly: `const auto base = texture_a.parent_path().parent_path(); std::filesystem::current_path(base);`. Build `relative_a` as `relative(texture_a, base)` (always `crate0/diffuse.png`) and derive `alternate_a` from it as today.
- **Done when:** The test passes with the source on C: and `-B D:\build\...` on Windows, and still passes on Linux/macOS. `grep -n "relative(.*current_path" tests/` is empty.

### T7 — Give every test its own fixture directory inside its build tree
- **Where:** `tests/CMakeLists.txt:1-6`; fixed names in `/tmp`: `AssetCameraDataTests.cpp:9`, `AssetLightDataTests.cpp:9`, `RenderSceneCameraApplyTests.cpp:18`, `RenderSceneLightApplyTests.cpp:18`, `SceneAssetRemovalBlockedTests.cpp:19`, `SceneInstantiationTests.cpp:19`, `ScenePurgeUnusedAssetsTests.cpp:19`, `SceneRemovalTests.cpp:19`, `SceneSystemTests.cpp:7`, `SceneTransformCompositionTests.cpp:45`
- **Problem:** Ten tests write fixed file names such as `/tmp/vve_scene_system_test.obj`. Two build trees running ctest at the same time race on them, and on a shared Linux machine a second user cannot overwrite the first user's file. The 3 capture tests use clock-suffixed temp dirs instead.
- **Change:** In `vve_add_engine_test`, add `target_compile_definitions(${name} PRIVATE VVE_TEST_TMP_DIR="${CMAKE_CURRENT_BINARY_DIR}/tmp/${name}")`. Each test calls `create_directories(VVE_TEST_TMP_DIR)` and writes its fixtures and captures there. This replaces `temp_directory_path()` in all 13 tests. A fixed per-test directory is preferred over random names because it is race-free across trees and leaves inspectable files after a failure.
- **Done when:** `grep -n temp_directory_path tests/*.cpp` is empty. `ctest -j8` in build/debug-linux and build/release-linux started at the same time passes twice in a row.

### T3 — Compare imported light/camera values against the fixture, including world transforms and the glTF range
- **Where:** `tests/AssetLightDataTests.cpp:8-23,35-45`; `tests/AssetCameraDataTests.cpp:56-68,80-93`; `src/versions/simple/Assets.ixx:307-312` (`globalTransform`), `:319-341` (`lightRange`, the `PBR_LightRange` path), `:343-362`, `:377-390`
- **Problem:** Every assertion (`kind`, colour > 0, finite values, fov > 0) also passes for a default-constructed descriptor. The fixture values (range 8, position, yfov 0.7) are never compared, so neither the world-space conversion nor the range import is tested.
- **Change:**
  - Both fixtures: put the light/camera node under a parent `Rig` with translation [10,0,0] and rotation [0,0.7071068,0,0.7071068] (90° about +Y). The child has translation [1,2,3].
  - Light fixture: add a second child, a spot light with `innerConeAngle` 0.2, `outerConeAngle` 0.6 and no range. Tolerance 1e-4.
  - Point light: position (13,2,−1); range 8 (PBR_LightRange); `color × intensity` == (0.75,1.5,2.25), because Assimp folds the glTF intensity into the colour.
  - Spot light: direction (−1,0,0), `cone` 0.6, `inner_cone` 0.2 (today's convention, see R13), range == `LightRange{}.value` (10).
  - Camera: position (13,2,−1), direction (−1,0,0), up (0,1,0), fov 0.7 ± 1e-3 (the default is 1.047), aspect 1.5, near 0.1, far 50.
  - Write the fixtures to the per-test directory from T7.
- **Done when:** Both tests pass. Negative check: replacing `globalTransform(...)` with the node's own `mTransformation` in `lightDescriptor`/`cameraDescriptor` makes both fail.

### T6 — Cover rotated and non-uniformly scaled parents in SceneTransformCompositionTests
- **Where:** `tests/SceneTransformCompositionTests.cpp:43-68` (translation-only fixture), `:96-100` (the expected value adds translations only); `src/versions/simple/Render/RenderSceneImport.cpp:18-49,62-99`
- **Problem:** Composition now uses matrices and `decomposeTransform`. The test still has only translated nodes and re-derives the expectation by adding translations. Rotation order, scale propagation and quaternion extraction are therefore untested.
- **Change:**
  - Fixture parent `Rig`: translation [2,3,4], rotation [0,0.7071068,0,0.7071068], scale [2,1,1]. Child A: translation [1,0,0]. Child B: translation [0,1,0], rotation [0,0.7071068,0,0.7071068]. Both use mesh 0.
  - Replace `expectedWorldTransform` with hard-coded values matched by node name:
    - A: T (2,3,2), R = parent rotation, S (2,1,1).
    - B: T (2,4,4), R = 180° about Y, i.e. (w 0, y ±1), S (1,1,2). These cases are shear-free, so decomposition is exact.
  - Compare quaternions with `|dot(q1,q2)| > 1 − 1e-4` (q and −q are the same rotation).
  - Write the fixture to the T7 directory.
- **Done when:** The test passes. Negative check: making `importedSceneWorldTransforms` compose translation only makes it fail.

### T2 — Test shadowing with a real occluder and stop enshrining the CPU bias
- **Where:** `tests/SimpleForwardRendererTests.cpp:189-197` (`hasConsistentGpuSample` recomputes `shadow_factor` with the renderer's own formula), `:212` (`sample.bias != 0.001F`), `:628` (the scene is a single plane); `src/versions/simple/Render/Renderer.ixx:79,90-91`; `src/versions/simple/Render/RendererDebug.cpp:16-44,62`; `src/versions/simple/shaders/simple_forward.slang:131-138,286-287,344-345`
- **Problem:** The test re-derives `shadow_factor` from the same inputs and formula the renderer used, so it can never disagree. Spot and point samples carry a fixed CPU bias of 0.001, while the shader uses `perspectiveShadowBias(far, w)`. The test asserts the 0.001. No scene in the test suite has an occluder, so a broken shadow pass or PCF compare still passes.
- **Change:**
  - New `tests/RenderShadowOcclusionTests.cpp` (white-box, hidden 128×128 window). Scene: an 8×8 grey plane at y=0, a unit cube at (0,1.5,0), and the camera at `lookAt((0,6,6),(0,0,0))`, so the cube does not hide the origin on screen.
  - Run once per light type, calling `clearScene` in between: directional (0,−1,0); spot at (0,5,0) aiming down, range 10, cone 0.5; point at (0,5,0), range 10.
  - With `setGpuDebugReadback(true)`, the origin sample of that type must have `has_gpu`, `gpu_depth < expected_depth − 0.001` (the occluder is nearer to the light) and `shadow_factor == 0.35`.
  - After `setObjectTransform` moves the cube to (3,1.5,0): `|gpu_depth − expected_depth| < 0.002` and the factor is 1.
  - Pixels: the mean luminance of a 5×5 patch at the image centre with the cube must be ≤ 0.6 × the value without it.
  - Port `perspectiveShadowBias` to C++ with its constants moved into `simple_shared.h`, and use it in `recordShadowDepthSamples`. Delete `shadowCompareBias`, the :212 assertion and the formula re-derivation in `hasConsistentGpuSample`; keep the `has_gpu` and finiteness checks.
- **Done when:** The new test passes on all three platforms. Negative check: making `pcfShadow` in the shader return 1.0 makes it fail. `grep -n "0.001F" tests/SimpleForwardRendererTests.cpp` no longer finds the bias assertion.

### T9 — Label the tests that need a window or GPU
- **Where:** `tests/CMakeLists.txt:1-50`; `examples/light_shadow_debug/CMakeLists.txt:16-19`
- **Problem:** No label separates CPU tests from tests that open windows and create a Vulkan device. So a machine without a GPU or display (CI, SSH) cannot run the CPU subset. The window/GPU tests are the six Render*/SimpleForwardRenderer tests, plus WindowInputTests, WindowOwnershipTests, WorldTests and UserSystemTests (`init()` with windows), plus the two example tests.
- **Change:** Give `vve_add_engine_test` a `GPU` keyword (parse it with `cmake_parse_arguments`) that sets `LABELS gpu`. Set `LABELS "gpu;example"` on the example tests.
- **Done when:** `ctest -LE gpu -N` lists only tests that open no window. On a machine without Vulkan, `ctest -LE gpu` passes.

### T10 — Run every example for a few frames under CTest
- **Where:** `examples/CMakeLists.txt`; currently only `examples/light_shadow_debug/CMakeLists.txt:16-19` and `tests/CMakeLists.txt:52-67` register example runs
- **Problem:** `game`, `testscene`, `sponza`, `simple_forward_demo` and `physics` are only compiled. A startup failure (asset path, scene load, renderer init) is found only by hand.
- **Change:** Add `vve_add_example_test(<target>)` in `examples/CMakeLists.txt`. It registers `<target> --frames 3` with `LABELS "gpu;example"` and `TIMEOUT 60`; call it for each example. Use `--output` / a capture path inside the build tree where an example writes files.
- **Done when:** `ctest -L example -N` lists all examples, and all of them pass. Negative check: renaming `assets/game` makes the `game` and `testscene` tests fail with exit 2.

## Phase 2 — Correctness and multi-window rendering

### W8 — Count a frame only when it was presented; error when the renderer is not initialised
- **Where:** `src/versions/simple/Render/RenderSystem.ixx:357-377` (both branches do `++rendered_frames_` and feed the FPS sample); `src/versions/simple/Render/Renderer.ixx:245` and `RendererDraw.cpp:22-141` (`drawFrame` returns `void`; early exits at :25 zero extent, :26/:29 sync/recreate, :42, :70, :74, :76, :93-99, :103, :117, :133-137)
- **Problem:** Partly fixed. The "no native window" case is gone: since #38, hidden windows are real SDL windows. `renderFrame` still counts a frame in two cases:
  - the renderer is not initialised (direct calls, or after `shutdown()`);
  - `drawFrame` bailed out (zero-size or minimized window, OUT_OF_DATE acquire, failed submit).

  `renderedFrameCount()` and `renderingFramesPerSecond()` therefore overstate what was presented. These values appear in the testscene/postprocessing GUI and the simple_forward_demo summary. Frame-count tests cannot catch a renderer that silently skips every frame.
- **Change:**
  - `ForwardRenderer::drawFrame` and `renderFrame` return `bool presented`: true only when `vkQueuePresentKHR` returned `VK_SUCCESS` or `VK_SUBOPTIMAL_KHR`.
  - `RenderSystem::renderFrame` returns `std::unexpected(Error::not_initialized)` when `!initialized_`, the same as `captureFrameToPng`.
  - It advances `rendered_frames_`, `render_fps_frames_` and the rendered-window count only when `presented`. A skipped frame is still a success. With several windows (W3b), a frame counts once when at least one window was presented, and the rendered-window count is the number of windows presented.
- **Done when:** New `tests/RenderFrameCountTests.cpp` (white-box):
  - Without GPU: a default `simple::RenderSystem` with a default `simple::WindowSystem` returns `Error::not_initialized` from `renderFrame(ws)`, and `renderedFrameCount()==0`. Today it is 1.
  - With GPU (`simple::Engine`, hidden 64x64 window): three `renderFrame()` calls give `renderedFrameCount()==3`.
  - `UserSystemTests` (`==2`) and `SimpleForwardRendererTests.cpp:680` (`==1`) still pass.

### W4 — Check Dear ImGui init results and set ready flags only on success
- **Where:** `src/versions/simple/GUI.ixx:61-66` (`initSDL`), `:94-99` (`initVulkan`), `:110-114` (`buildFonts`); `src/versions/simple/Engine.ixx:170-179`
- **Problem:** In ImGui 1.91.9, `ImGui_ImplSDL3_InitForVulkan`, `ImGui_ImplVulkan_Init` and `ImGui_ImplVulkan_CreateFontsTexture` return `bool`. The code drops the result and sets `sdlBackendReady_`, `vulkanBackendReady_` and `fontsReady_` anyway. A failed backend is then driven by `recordFrame` (`ImGui_ImplVulkan_NewFrame`) and by `shutdownVulkan`, which asserts or crashes. When `makeGuiInitInfo()` returns `nullopt`, the GUI is silently off for good (`gui_sdl_initialized_ = true`), with no message.
- **Change:**
  - `initSDL`, `initVulkan` and `buildFonts` return `std::expected<void, Error>`: `nullptr` gives `invalid_argument`, a backend that is not ready gives `not_initialized`, and ImGui returning `false` gives `platform_error`.
  - Set each flag only on success, and add `[[nodiscard]] bool ready() const`.
  - In `simple::Engine::renderFrame`, check every step and treat a `nullopt` info as a failure. On failure, print one `[vve::simple] GUI disabled: <stage>` line, shut down the parts that succeeded, do not install the record sink, and keep rendering.
  - Failing `step()` instead is rejected: apps that never draw a GUI would stop because of a GUI-only problem.
- **Done when:**
  - `tests/GuiSystemTests.cpp`: `initSDL(nullptr)` and `initVulkan(nullptr)` return `invalid_argument`; `buildFonts()` before `initVulkan` returns `not_initialized`; `ready()` is false; `recordFrame` still does not call the callback.
  - `SimpleForwardRendererTests.cpp` asserts `engine.gui().ready()` after the first `engine.renderFrame()`.

### B5 — Keep the post-processing callback; stop exceptions escaping and drop the dead resize branch
- **Decision DEC-8** (see the table above).
- **Where:** `src/versions/simple/GUI.ixx:117-124` (`GuiSystem::recordFrame` calls the user's GUI callback), `src/RenderSystem.ixx:45` (`setPostProcessSetup`, undocumented lifetime), `examples/postprocessing/postprocessing.cpp:139-155` (the `else` branch "after a resize")
- **Problem:**
  - The setup callback runs once, when the renderer starts. The example keeps pointers to the chain and to each effect's settings, and its GUI changes and adds or removes effects through them at runtime. That works and stays as it is, but the API does not say that the pointers stay valid (a resize keeps the chain).
  - vvppl throws `std::runtime_error` when it cannot create an effect's pipeline. When that happens in `add*()` inside the GUI callback, the exception leaves `engine.step()` while a command buffer is being recorded.
  - The example's resize branch never runs, because the callback is not called again on resize.
- **Change:**
  - Document on the facade and simple `setPostProcessSetup`: the callback runs once when the renderer starts; the `PostProcessing` reference and the settings references it hands out stay valid until the engine shuts down, including across resizes, so they may be kept and changed at any time.
  - In `GuiSystem::recordFrame`, catch `std::exception` thrown by the user callback: log the message once, end the Dear ImGui frame with its error recovery (`ImGui::ErrorRecoveryStoreState` before the callback, `ImGui::ErrorRecoveryTryToRecoverState` after the catch; bundled ImGui is 1.91.9), and make the next `step()` return `Error::platform_error`.
  - Delete the example's `else` branch, and the `chain == nullptr` check that only exists for it.
- **Done when:**
  - New white-box test `tests/GuiCallbackErrorTests.cpp` (hidden window): a GUI callback that opens an ImGui window and throws `std::runtime_error` makes `step()` return `Error::platform_error` instead of throwing, and the process keeps running (a following `step()` without the throwing callback succeeds).
  - `PostProcessingSmokeTests` still passes, and the example still changes effects with the sliders and checkboxes (check once by hand).
- **Risk / notes:** Keep vvppl linked PUBLIC, because the facade exposes `vvppl::PostProcessing`.

### W2 — Make simple::WindowSystem::init transactional and idempotent
- **Where:** `src/versions/simple/Window.ixx:336-404` (SDL init :356-360, early failure return :378-381, commit :399-400); `src/versions/simple/Engine.ixx:126-132`
- **Problem:** If window k fails to open, `init` returns but keeps windows 0..k-1 and leaves video initialised. `simple::Engine::init()` leaves `initialized_` false, so a retry has two effects:
  - `SDL_InitSubSystem` runs again. Its refcount becomes 2, but `~Impl` quits only once.
  - A second full set of windows with the same ids is appended. `findWindow("main")` then returns the stale window, and the renderer binds to it.

  `WindowSystem::init` also has no already-initialised guard and accepts duplicate ids.
- **Change:** In `WindowSystem::init`:
  - Return `Error::already_initialized` if `impl_->video_initialized || windowCount() > 0`.
  - Validate all `WindowDesc` before any SDL call: a duplicate `id` gives `Error::duplicate_object`; an empty id or a zero extent gives `Error::invalid_argument`.
  - Create the windows into a local `std::vector<Window>` and a local id-to-index map. On failure, let RAII destroy them, and call `SDL_QuitSubSystem(SDL_INIT_VIDEO)` if this call did the init.
  - Move the windows into `impl_->windows` / `impl_->indices` only after all of them succeeded.
- **Done when:** New `tests/WindowSystemInitTests.cpp` (white-box `vve::simple::WindowSystem`, hidden 64x64 windows):
  - `init` with two descs that both have id "a" returns `duplicate_object`, and `windowCount()==0`. Today it succeeds with 2 windows.
  - A following `init` with one valid desc succeeds, and `windowCount()==1`.
  - A third `init` returns `already_initialized`, and the count stays 1.

  Failing in the middle of window creation has no deterministic trigger, so that rollback path is covered by review only.

### W11 — Do not re-run succeeded user-system init hooks when facade init is retried
- **Where:** `src/Engine.ixx:363-370` (`init`), `:398-403` (`initSystems`)
- **Problem:** If the init of user system k fails, `systems_initialized_` stays false, and the next `engine.init()` restarts `initSystems()` at the first system. Systems 0..k-1 then run `init` twice; for example, `UserSystemTests`' `CountingSystem::init` would add a second plane. This is the facade-level twin of W2.
- **Change:** Replace `bool systems_initialized_` with `std::size_t systems_initialized_count_`. `initSystems()` walks the tuple by index (`std::index_sequence`), skips indices below the count, and increments the count after each success. `init()` is complete when the count equals `sizeof...(TSystems)`.
- **Done when:** New case in `tests/UserSystemTests.cpp` (hidden window, no GPU needed) with system A (counts its inits) and system B (fails its first init). The first `engine.init()` fails, the second succeeds, and the counts are `A == 1` (today 2) and `B == 2`.

### W9 — One owner for the frame index and delta (simple::Engine); the facade reads it
- **Where:** `src/Engine.ixx:127-128` (facade `last_frame_time_`, `frame_`), `:363-370`, `:381-396` (`step`); `src/versions/simple/Engine.ixx:61-62`, `:129`, `:147-159`
- **Problem:** The facade keeps `frame_` and `last_frame_time_` for the user systems' `FrameContext`. `simple::Engine` keeps its own `frame_` for the `MaxFrames` stop, plus a `last_frame_time_` that nothing reads. The simple `frame_` advances in `engineStep` before updates and rendering. When `updateSystems` or `engineRenderFrame` fails, the facade counter does not advance. So after an error, `frame_index` lags the counter that decides `MaxFrames`, and the next delta spans two polls. The first delta also includes whatever the app did between `init()` and the first `step()`. Loop-style examples (all of them) cannot get the engine delta and time themselves (`examples/game/game.cpp:227-232`).
- **Change:**
  - `simple::Engine::step()` computes `FrameContext{frame_index = frame_ before the increment, delta = now - last_frame_time_}`. The first step uses `DeltaTime{}`.
  - It stores the result and exposes `frameContext() const -> FrameContext`.
  - The facade deletes its own `frame_` and `last_frame_time_`, adds `detail::engineFrameContext(const EngineState&)` in `src/Engine.cpp`, uses it in `step()`, and exposes `Engine::frameContext() const`. W10 needs this.
- **Done when:**
  - `tests/UserSystemTests.cpp` gets a system whose first `update` returns an error. With `MaxFrames{2}`: `step()` #1 fails; `step()` #2 returns `stopped`; that update sees `frame_index == 1` (today 0) and `delta_time.seconds > 0`.
  - `engine.frameContext().frame_index.value == 1`.
  - The existing `last_frame == 1` check stays green.
  - `grep -n "last_frame_time_" src/Engine.ixx` finds nothing.

### W14 — Make simple::Engine::run render frames
- **Where:** `src/versions/simple/Engine.ixx:135-144`
- **Problem:** `run()` only loops `step()` and never calls `renderFrame()`. A white-box program using it gets windows that stay empty, and without `MaxFrames` it spins polling until a window is closed. The review listed this under dead code, but the function is still there unchanged.
- **Change:** Mirror the facade loop: `step()`, then `renderFrame()` (return its error), then stop on `FrameStatus::stopped`. Deleting the function is the alternative; keeping it is preferred because white-box tests drive `simple::Engine` directly.
- **Done when:** GPU white-box case in `tests/RenderFrameCountTests.cpp`: `simple::Engine` with one hidden 64x64 window and `MaxFrames{2}`. `run()` succeeds and `renderSystem().renderedFrameCount() == 2` (today 0).

### W10 — Make DefaultCameraController frame-rate independent
- **Where:** `src/Window.ixx:135-182` (`move_step`/`turn_step` :142-143, `update` :152); callers `examples/sponza/sponza.cpp:107`, `examples/game/game.cpp:237`, `examples/postprocessing/postprocessing.cpp:289`, `examples/testscene/testscene.cpp:358`; present mode `src/versions/simple/Render/Renderer.ixx:198-204` with `src/versions/simple/CMakeLists.txt:46`
- **Problem:** `move_step` (0.08) and `turn_step` (0.025) are applied once per `update()` call, so the speed depends on the frame rate. Debug presents with FIFO (monitor rate), every other config with MAILBOX (uncapped). The same example therefore moves at about 5 units/s in Debug on a 60 Hz screen and many times faster in Release. `game.cpp` already moves its crates with a clamped `dt`, but not the camera.
- **Change:**
  - Change the signature to `update(const InputState &, DeltaTime dt)`.
  - Replace the fields with `move_speed` (units/s, default 4.8) and `turn_speed` (rad/s, default 1.5), and clamp `dt` to 0.1 s.
  - Pass `engine.frameContext().delta_time` (W9) in the four examples; `game.cpp` can use its existing `dt`.
  - Move the member definition from the facade interface into `src/Window.cpp`, because the facade must stay implementation-free.
- **Done when:** `tests/DefaultCameraControllerTests.cpp`:
  - With `move_speed = 2`, one update with W held and `DeltaTime{0.5}` moves the eye by 1 along forward.
  - Two updates with `DeltaTime{0.25}` reach the same eye position.
  - Yaw is checked the same way with `turn_speed`.
  - `DeltaTime{10}` moves at most `move_speed * 0.1`.
  - The existing basis, shift and pitch-clamp cases pass `DeltaTime{1}`.
- **Risk / notes:** This is a source break for `update(input)` callers. It depends on W9 for `frameContext()`.

### W3a — Split ForwardRenderer into shared device state and per-window render targets
- **Decision DEC-2** (see the table above).
- **Where:** `src/versions/simple/Render/Renderer.ixx` (`ForwardRenderer` members), `RendererResources.cpp` (`init`, `recreateSwapchain`, `cleanup`), `RendererDraw.cpp` (`drawFrame`, `recordCommandBuffer`), `RendererDebug.cpp` (capture), `src/versions/simple/Engine.ixx:162-184` (renders only the first window with a native handle)
- **Problem:** Only the first window ever gets a surface and swapchain; the others are created with `SDL_WINDOW_VULKAN` and never presented, although the API (`WindowSetups`, per-window `renderer_id`, per-window `WindowFrameData`) offers several windows. Everything window-specific is mixed into `ForwardRenderer` next to the device-wide resources, so a second window cannot be added.
- **Change:** Pure refactor, one window, no behaviour change.
  - Shared device state stays in `ForwardRenderer`: instance, physical device, device and queues, allocator, descriptor-set layout, pipeline layout, shader modules, pipelines, shadow arrays, textures, materials, meshes, command pool.
  - A new `WindowTarget` owns everything per window: `SDL_Window*`, surface, swapchain and image views, depth and HDR images, frame sync, command buffers, per-frame uniform buffers and descriptor sets, the post-processing chain, the camera (eye, target, fov, clip), `requestedExtent`, `lastRenderedImageIndex`.
  - `drawFrame(WindowTarget&)`, `recreateSwapchain(WindowTarget&, ...)` and capture take the target. The forward and GUI pipelines must not depend on one window's extent: do R9 (dynamic viewport and scissor) first.
- **Done when:** all tests pass unchanged on the three platforms, and `grep` shows no swapchain, surface, depth, HDR, frame-sync or per-frame uniform member left directly in `ForwardRenderer`.
- **Risk / notes:** Large mechanical change; keep it free of behaviour changes so regressions are easy to bisect. Depends on R9.

### W3b — Render every window
- **Decision DEC-2** (see the table above).
- **Where:** `src/versions/simple/Engine.ixx` (`renderFrame`), `src/versions/simple/Render/RenderSystem.ixx` (`initialize`, `renderFrame`, `lastRenderedWindowCount`), `Renderer.ixx` (`WindowTarget` list), `src/versions/simple/Window.ixx`, facade `src/Window.ixx` (`WindowSetup::renderer` docs), `docs/notes/architecture.md`
- **Problem:** After W3a the renderer can hold several targets but still renders one.
- **Change:**
  - Rule: every window renders with the forward renderer, unless its renderer id is `"none"`. An empty id and `"forward"` both mean the forward renderer; any other id makes `init()` return `Error::invalid_argument`. Document the rule on `vve::WindowSetup::renderer` and `simple::WindowDesc::renderer_id`.
  - The first rendered window creates the device (its surface is used for device selection). Every further window adds a `WindowTarget`; its surface must be presentable from the graphics queue family, otherwise `init()` returns `Error::platform_error` with a clear message.
  - `renderFrame` renders all targets in window order: per window, prepare that window's frame uniforms and directional cascades from its camera, record its shadow passes and forward pass, post-process, present. The shared shadow arrays are reused per window; the existing begin barriers already order one window's shadow writes after the previous window's reads.
  - Each window resizes independently. A closed window's target is destroyed after `vkDeviceWaitIdle`; the engine keeps its current stop rule (`anyShouldClose`) unless a task changes it.
  - Dear ImGui stays bound to the first rendered window: its GUI pass runs only there; events of all windows still reach `InputState`.
  - Post-processing: each window gets its own vvppl chain, and the `setPostProcessSetup` callback runs once for each window when that window's target is created (for the first window this is today's behaviour). Update the `setPostProcessSetup` documentation from B5 accordingly.
  - `lastRenderedWindowCount()` is the number of windows presented in the last frame. The facade gets `captureFrameToPng(WindowHandle, path)`; the existing overload captures the first rendered window.
- **Done when:**
  - New GPU test `tests/MultiWindowRenderTests.cpp` (`simple::Engine`, hidden windows "a" 64×64 and "b" 96×64, one cuboid, default camera): after two `renderFrame()` calls, `lastRenderedWindowCount() == 2`, and both captures exist with their own sizes and show the cuboid at the centre.
  - A third window with renderer `"none"` is not rendered (count stays 2); a window with renderer `"foo"` makes `init()` fail with `invalid_argument`.
  - Resizing only "b" (`SDL_SetWindowSize` + `SDL_SyncWindow`) changes only b's capture size.
  - `WindowOwnershipTests` asserts the presented-window count instead of the open-window count.
- **Risk / notes:** Some drivers ignore resizes of hidden windows; then call `recreateSwapchain` on b's target directly and check the extent. Coordinate with W8 (count only presented frames) and W6 (the count then needs no window snapshot).

### W3c — One camera per window; remove the ECS window-camera API
- **Decision DEC-2 and DEC-4** (see the table above).
- **Where:** facade `src/RenderSystem.ixx:46` and `src/RenderSystem.cpp:49` (`setCamera(Camera, PixelExtent)`), simple `RenderSystem::setCamera` (`RenderSystemScene.cpp`), `ForwardRenderer::setCamera`; window-camera API: `src/Window.ixx:195,223-230`, `src/Window.cpp:134-169`, `src/versions/simple/Window.ixx` (`WindowInfo::camera`, `setWindowCamera`, `clearWindowCamera`, `windowCamera`, `setActiveCamera`, `activeCamera`), their sections in `WorldTests` and `WindowOwnershipTests`; all examples that call `setCamera`
- **Problem:** Every window needs its own view. Today there is one renderer camera set by `RenderSystem::setCamera`, whose `PixelExtent` argument is ignored. In parallel, `WindowSystem` stores an ECS `Entity` per window as its "camera", which nothing renders through.
- **Change:**
  - Facade and simple: `setCamera(Camera)` sets the default camera, used by every window without its own; `setCamera(WindowHandle, Camera)` sets one window's camera, and returns `Error::invalid_handle` for an unknown window. The aspect ratio comes from each window's extent. Remove the `PixelExtent` parameter.
  - Delete the ECS-based window-camera API listed above, `WindowInfo::camera` and `Window::camera()`.
  - Update every example to the new `setCamera` and `README.md`/`examples/FACADE_AUDIT.md`.
- **Done when:** in `MultiWindowRenderTests`, window "a" uses the default camera looking at the cuboid and window "b" gets its own camera looking away; a's capture shows the cuboid at the centre and b's centre pixel is the background colour. The removed names grep empty in `src/`, `tests/` and `examples/`.
- **Risk / notes:** Public API change; every example changes. Do D11 after this task (D11 no longer touches the window-camera API or `setCamera`'s signature).

### W12 — Skip minimized windows and stop busy-looping while every window is minimized
- **Decision DEC-2** (see the table above).
- **Where:** `src/versions/simple/Engine.ixx` (`renderFrame`); `src/versions/simple/Render/RendererDraw.cpp` (`drawFrame`); `src/versions/simple/Window.ixx` (`minimized` is tracked, but nothing reads it)
- **Problem:** Nothing reads `WindowInfo::minimized`. `poll()` does not block, and `drawFrame` either returns at once (zero extent) or fails in acquire/recreate on platforms that keep the old size. So while a window is minimized, its frame fails or logs errors, and when every window is minimized `step()` spins at 100% of a core and (see W8) still counts frames.
- **Change:** In `simple::Engine::renderFrame`, skip every window that is minimized or whose `SDL_GetWindowSizeInPixels` is 0. If no window is left, skip rendering, block in `SDL_WaitEventTimeout(nullptr, 100)` and return success. With a null event pointer this call wakes on the restore event and leaves it queued for the next `poll()`.
- **Done when:**
  - GPU white-box case in `tests/RenderFrameCountTests.cpp` with two hidden windows: mark "b" minimized; `renderFrame()` succeeds and presents 1 window. Mark both minimized; `renderFrame()` succeeds, presents 0 windows and does not count a frame. Clear the flags; both render again.
  - Manually, a minimized Release `testscene` uses under 5% CPU and renders again after restore.
- **Risk / notes:** Depends on W8 (count only presented frames) and W3b.

### R10 — Bound the frame fence and acquire timeouts and skip the frame on timeout
- **Where:** `S/Render/RendererDraw.cpp:41-42,73-76`
- **Problem:** `vkWaitForFences` and `vkAcquireNextImageKHR` wait with `UINT64_MAX`. When the presentation engine never releases an image (hidden, minimised or occluded windows with FIFO, e.g. on Wayland or macOS), `drawFrame` blocks forever, and the event loop and GPU tests hang instead of skipping frames.
- **Change:** Add constants `kFrameFenceTimeoutNs` (1 s) and `kAcquireTimeoutNs` (100 ms). On `VK_TIMEOUT` from the fence wait, return before touching frame state. On `VK_TIMEOUT`/`VK_NOT_READY` from acquire, return; the semaphore is not signalled on these codes, so nothing must be consumed. Count both in `skippedFrameCount()` instead of calling `reportFrameFailure`. Leave `submitOnce` as it is (one-time uploads always finish). Optionally skip `drawFrame` when the window is `SDL_WINDOW_MINIMIZED | SDL_WINDOW_OCCLUDED`.
- **Done when:** a pure function `acquireAction(VkResult)` → {render, skip, recreate, fail} is tested in a new non-GPU test: `VK_TIMEOUT`/`VK_NOT_READY` → skip, `VK_SUBOPTIMAL_KHR` → render, `VK_ERROR_OUT_OF_DATE_KHR` → recreate, `VK_ERROR_DEVICE_LOST` → fail. `grep -n UINT64_MAX src/versions/simple/Render` prints nothing. `SimpleForwardRendererTests` asserts `skippedFrameCount() == 0`.
- **Risk / notes:** `vkQueuePresentKHR` can still block (Mesa Wayland FIFO without fifo-v1), and timeouts do not cover that. `RenderSystem::renderFrame` (`S/Render/RenderSystem.ixx:361-363`) still counts skipped frames as rendered; decide whether it should.

### R16 — Render Dear ImGui through a UNORM view instead of the sRGB swapchain view
- **Where:** `S/Render/RendererDraw.cpp:386-392`, `S/Render/RenderSystem.ixx:279-289`, `S/Vulkan/Presentation.ixx:173-188`
- **Problem:** ImGui's colours and font atlas are authored in sRGB and its backend writes them unchanged. The GUI pass renders into the `*_SRGB` swapchain view with an sRGB pipeline format, which encodes them a second time: the GUI looks washed out (ImGui grey 128 lands at about 188) and text anti-aliasing blends in the wrong space.
- **Change:** Preferred: when `VK_KHR_swapchain_mutable_format` is available, create the swapchain with `VK_SWAPCHAIN_CREATE_MUTABLE_FORMAT_BIT_KHR` plus `VkImageFormatListCreateInfo{SRGB, UNORM}`, add a UNORM view per image for the GUI pass, and give ImGui the UNORM format in `PipelineRenderingCreateInfo`. Fallback: convert `ImGui::GetStyle().Colors` to linear once in `GuiSystem::initContext`; this fixes the theme but not user colours.
- **Done when:** a GUI capture test (hidden window; new file or `GuiSystemTests` extended) fills the viewport with `GetBackgroundDrawList()->AddRectFilled(..., IM_COL32(128,128,128,255))` and asserts the captured pixel is 128±2 per channel (today about 188).
- **Risk / notes:** Low priority and purely visual. The device extension must be checked on MoltenVK and KosmicKrisp.

### W1 — Mint render-object and scene-instance handles from the global counter
- **Where:** `src/versions/simple/Render/RenderSystem.ixx:198-199` (`next_render_object_id_`, `next_scene_instance_id_`), `:230-237` (`registerRenderObject`), `src/versions/simple/Render/RenderSceneImport.cpp:257-258` (`instantiateScene`); global counter in `src/Handle.ixx:49-60`
- **Problem:** Every `RenderSystem` numbers `RenderObjectHandle` and `RenderSceneInstanceHandle` from 1 with its own counter. All other handles come from the process-wide `makeCounterHandle`. With two engines in one process, a handle from engine A is accepted by engine B and acts on B's own object or instance, where it should fail with `missing_object`.
- **Change:** Use `makeCounterHandle<RenderObjectHandle>()` and `makeCounterHandle<RenderSceneInstanceHandle>()`, and delete both member counters. `detail::lightSceneOwner` (sets bit 62) and the owner keys that use `instance.value` keep working, because counter handles never set bit 62.
- **Done when:** New `tests/RenderHandleUniquenessTests.cpp` (facade, no `init()`, no GPU). It creates two `vve::Engine<>`, each calls `addPlane`, and asserts: the handles differ; `b.objectTransform(hA)` and `b.removeObject(hA)` return errors; `a.objectTransform(hA)` still succeeds. It loads a tiny OBJ into both engines (pattern of `SceneInstantiationTests.cpp`) and asserts that `b.removeSceneInstance(instanceA)` fails while `b`'s own instance survives.
- **Risk / notes:** `SimpleForwardRendererTests.cpp:371,409,428` use `makeHandleForTest<RenderObjectHandle>(9'999U)` as the "missing" handle. With the global counter a live handle can reach 9999, so change these to `makeHandleForTest<...>(RenderObjectHandle::id_mask)`.

### W5 — Default WindowSetups / simple::Windows to empty; add the "main" window only in applyDefaults
- **Where:** `src/Window.ixx:76` (`value_{WindowSetup{}}`), `:62-68` (`clear()` workaround in the init-list constructor); `src/Engine.ixx:208-213` (`EngineBuilder::addWindow` workaround); `src/versions/simple/Window.ixx:38`; `src/versions/simple/Engine.ixx:232`
- **Problem:** `vve::WindowSetups ws; ws.add(WindowSetup{}.id("tools"));`, passed through `EngineBuilder::windows(ws)` or `Engine{..., ws}`, opens two windows ("main" and "tools"). `simple::Windows w; w.value.push_back(desc)` does the same. Only the init-list constructor and `addWindow` work around this. With W3b, the unwanted "main" window would also be rendered.
- **Change:**
  - Make `std::vector<WindowSetup> value_{}` and `Vector<WindowDesc> value{}` empty by default.
  - Drop `value_.clear()` in the init-list constructor, and the `windows_configured_` special case in `addWindow` (plain `windows_.add`).
  - The single default "main" window then comes only from `simple::Engine::applyDefaults` (`Engine.ixx:232`) when the list is empty. The facade reaches this too, because `implementationWindows` yields an empty list.
- **Done when:** New `tests/WindowSetupDefaultsTests.cpp` (hidden windows, no GPU):
  - `WindowSetups ws; ws.add(<hidden "tools" 64x64>)`, then `EngineBuilder<>{}.windows(ws).build()` and `init()`: `windowCount()==1`, `findWindow("tools")` is present, `findWindow("main")` is empty. Today there are 2 windows.
  - `vve::simple::Windows{}.value.empty()`.
  - All existing window tests pass unchanged.

### W7 — Report mouse position and delta in normalised window coordinates (0..1)
- **Decision DEC-3** (see the table above).
- **Where:** `src/versions/simple/Window.ixx` (`WindowSystem::poll`, mouse motion handling; `InputState::setMousePosition`/`addMouseDelta`), facade `src/Window.ixx` (`InputState` accessors, extent comments), `src/Engine.ixx` (extent comment)
- **Problem:** Mouse position and delta are raw SDL window coordinates, while `WindowInfo::extent` is in pixels, and the facade states no unit. Consumers would have to know the window size in both units and the pixel density to use the mouse. Nothing reads mouse data yet: `DefaultCameraController` and the examples use the keyboard only.
- **Change:**
  - In `WindowSystem::poll`, divide motion `x` and `xrel` by the window width and `y` and `yrel` by the window height, both from `SDL_GetWindowSize` (window coordinates, so pixel density cancels out). Store the results. Mouse wheel stays in scroll ticks.
  - Document on the facade `InputState`: `mousePosition` is normalised window coordinates, (0,0) top left, (1,1) bottom right, y down; values can leave 0..1 while a drag continues outside the window. `mouseDelta` is in the same units. `mouseWheelDelta` is in scroll ticks.
  - Fix the extent comments: `WindowSetup::extent` is the initial size in window coordinates; `Window::extent()` is the drawable size in pixels.
- **Done when:** new white-box case in `tests/WindowSystemInitTests.cpp` (from W2; link `SDL3::SDL3` through `vve_add_engine_test`): create a hidden window of size w×h, `SDL_PushEvent` a `SDL_EVENT_MOUSE_MOTION` for its window id with x = w/2, y = h/4, xrel = w/10, yrel = −h/5, call `poll()`, and assert `mousePosition == (0.5, 0.25)` and `mouseDelta == (0.1, −0.2)` within 1e-5.
- **Risk / notes:** Mouse-look code must scale the x delta by the aspect ratio if it wants equal angles per distance; say so in the `InputState` comment.

### R12 — Require Vulkan 1.3
- **Decision DEC-1** (see the table above).
- **Where:** `src/versions/simple/Vulkan/Device.ixx` (`selectIfSuitable`, `supportsRequiredFeatures`, `VulkanDevice::create`), `src/versions/simple/Render/RendererDraw.cpp` (`vkCmdBeginRendering`/`vkCmdEndRendering`), `src/versions/simple/Render/RendererResources.cpp` (ImGui `ApiVersion`)
- **Problem:** Device selection checks only the `dynamicRendering` feature. A Vulkan 1.2 device that exposes `VK_KHR_dynamic_rendering` reports that feature (the KHR struct is an alias with the same sType) and is accepted. `VulkanDevice::create` then enables the feature without the extension, and the renderer calls the core 1.3 `vkCmdBeginRendering`, which such a device does not provide, so it crashes on the first frame.
- **Change:** In `selectIfSuitable`, reject devices whose `VkPhysicalDeviceProperties::apiVersion` is below 1.3, and print one line naming the device and its version when no device qualifies. Move the checks into a pure function `evaluateDeviceRequirements(const DeviceCapabilities&)` (apiVersion, swapchain extension, dynamicRendering feature, shaderSampledImageArrayDynamicIndexing) so they can be tested without a GPU. Pass `VK_API_VERSION_1_3` as ImGui's `ApiVersion`.
- **Done when:** a new non-GPU test `tests/VulkanDeviceRequirementsTests.cpp` checks: 1.3 + all features → accepted; 1.2 + `VK_KHR_dynamic_rendering` + feature → rejected; 1.3 without dynamicRendering → rejected; 1.3 without dynamic indexing → rejected. All existing tests pass on the three platforms.
- **Risk / notes:** Turns a first-frame crash on old drivers into a clear "no suitable device" error.

## Phase 3 — GPU cost

### R5 — Wait for acquire only where the swapchain image is first written; skip shadow samples without readback
- **Where:** `S/Render/RendererDraw.cpp:105,351-353,68`, `S/Render/RendererDebug.cpp:16-44`, `src/RenderSystem.ixx:128`
- **Problem:** `pWaitDstStageMask = TOP_OF_PIPE` makes every stage of the frame, including all shadow passes and the forward pass, wait for the presentation engine, so shadow work cannot overlap the acquire. `recordShadowDepthSamples` projects and allocates samples every frame even though only the readback path (`gpuDebugReadback_`) uses them.
- **Change:** Set the wait stage to `VK_PIPELINE_STAGE_TRANSFER_BIT`, because the swapchain image is first written by a blit in both paths. Add `TRANSFER` to the `srcStageMask` of the post-forward barrier call (:351) so that the swapchain `UNDEFINED→GENERAL` transition chains after the semaphore wait. `COLOR_ATTACHMENT_OUTPUT` also works but would make the forward pass wait. Call `recordShadowDepthSamples` only when `gpuDebugReadback_` is set, otherwise `shadowDepthSamples.clear()`. Document on the facade `shadowDepthSamples()` that the result is empty unless `setShadowDepthReadback(true)` was called.
- **Done when:** `SimpleForwardRendererTests` renders one frame with readback off and asserts `renderer.shadowDepthSamples.empty()`; the existing Debug checks with readback on still pass. With R14 sync validation, `SimpleForwardRendererTests`, `RenderLightingCaptureTests` and `PostProcessingSmokeTests` report 0 errors.
- **Risk / notes:** Do this together with R8, which restructures the same barriers.

### R8 — Skip the GUI pass when nothing is drawn and bypass vvppl for an empty chain
- **Where:** `S/Render/RendererDraw.cpp:356-415`, `S/GUI.ixx:116-125`, `S/Engine.ixx:177`, vvppl `src/VVPPL.cpp:479,531` (tag v1.0)
- **Problem:** `Engine::renderFrame` always installs a GUI sink, and `recordCommandBuffer` always records the GUI barrier plus a full-screen LOAD/STORE rendering pass on the swapchain image, even when no GUI callback exists. With post-processing configured but an empty chain, `vvppl::apply` blits HDR→internal→swapchain: two full-screen blits where the renderer's own path needs one.
- **Change:** Split `GuiSystem::recordFrame` into `prepareFrame() -> bool` (NewFrame, callback, Render; returns `ImGui::GetDrawData()->TotalVtxCount > 0`) and `record(cmd)`, and give `ForwardRenderer` both sinks. `drawFrame` calls `prepare` before recording. When it returns false, skip the GUI barrier and pass and transition the swapchain image `GENERAL→PRESENT_SRC` from `TRANSFER`/`TRANSFER_WRITE`. Add `[[nodiscard]] bool empty() const noexcept` to vvppl (new tag; update `GIT_TAG` in `CMakeLists.txt:177`) and take the direct-blit branch when `!postProcess || postProcess->empty()`.
- **Done when:** new `RecordedPass::gui` and `RecordedPass::post_process` tags exist. `SimpleForwardRendererTests` asserts no `gui` tag in a frame without `engine.gui().draw(...)` and exactly one after a callback that opens a window. `GuiSystemTests` is updated to the split API. R14 reports no errors with and without GUI and post-processing.
- **Risk / notes:** Needs an upstream vvppl change. Coordinate with R5.

### R7 — Remove dead shader work: unused colour varying, late unlit exit, redundant normalises
- **Where:** `slang:83,92,197` (varying), `slang:351-354` (unlit), `slang:298,337` (normalise), `slang:242-246` (camera position), `S/Vulkan/Pipeline.ixx:80`
- **Problem:** `vertexMain` reads the material SSBO into `color`, which `fragmentMain` never uses. The `unlit` early-out comes after all texture samples, the point, directional and spot loops and every PCF tap, so unlit objects pay the full lighting cost. Light directions that the CPU already normalised (`RendererShadowPrep.cpp:36,94`) are normalised again per fragment, and the camera position is rebuilt from the view matrix per fragment.
- **Change:** Remove `color` from both `VertexOutput` and `FragmentInput` (both together, or the interfaces mismatch) and the SSBO read in `vertexMain`. Set the `materials` binding in `kDescriptorSetBindings` to `VK_SHADER_STAGE_FRAGMENT_BIT` only. Move `if (object.unlit != 0)` directly after the base-colour sample (:216-219); `unlit` is a push constant, so the return is uniform control flow and implicit-LOD sampling stays valid. Use `-frame.directionalLightDirections[i].xyz`, `frame.spotLightDirections[i].xyz` and `-spotLightDir` without `normalize`. Optionally add a `float4 cameraPosition` to `FrameUniforms` on both sides and update the `static_assert`.
- **Done when:** `grep -n COLOR0 src/versions/simple/shaders/simple_forward.slang` prints nothing. A `static_assert` in `Pipeline.ixx` pins the materials binding to the fragment stage. `RenderLightingCaptureTests` adds a full-view unlit plane with base colour (0.5, 0.25, 1.0) and asserts the centre pixel is (188, 137, 255) ±2. All capture tests pass unchanged.

### R9 — Dynamic viewport/scissor and pass oldSwapchain on recreate
- **Where:** `S/Vulkan/Pipeline.ixx:252-253,275-277`, `S/Render/RendererResources.cpp:449,454-459,478-481`, `S/Vulkan/Presentation.ixx:71,128`
- **Problem:** The viewport and scissor are baked into the forward pipeline, so every resize destroys and recompiles it. The swapchain is destroyed before the new one is created and `oldSwapchain` is always `VK_NULL_HANDLE`, so the presentation engine cannot hand over images smoothly (flicker and extra stall on resize).
- **Change:** Add `VkPipelineDynamicStateCreateInfo{VIEWPORT, SCISSOR}` in `VulkanGraphicsPipeline::create` (viewport state with count 1 and null pointers) and drop the `extent` parameter. Record `vkCmdSetViewport`/`vkCmdSetScissor` after binding the shadow pipeline (resolution) and the forward pipeline (`swapchain.extent`). Remove the pipeline rebuild from `recreateSwapchain`. `VulkanSwapchain::create` takes an `oldSwapchain` handle, no longer calls `cleanup()` first, creates with `.oldSwapchain = old` and destroys the old handle after the new one exists.
- **Done when:** a new diagnostic `forwardPipelineCreateCount()` is added. A new `tests/RenderResizeTests.cpp` (hidden 64×64 window) renders, calls `SDL_SetWindowSize(96, 80)` + `SDL_SyncWindow`, renders 2 frames and asserts `swapchain.extent == currentWindowPixelExtent()`, a `captureFrameToPng` PNG of that size, `forwardPipelineCreateCount() == 1`, and 0 R14 errors.
- **Risk / notes:** Some drivers (Wayland, offscreen) ignore resizes of hidden windows. In that case the test calls `recreateSwapchain(currentWindowPixelExtent())` directly and checks only the pipeline count.

### R1 — Allocate and clear only the shadow layers that active lights use
- **Where:** `S/Render/RendererResources.cpp:98-107`, `S/Render/RendererDraw.cpp:194-255`, `S/Render/RendererShadowPrep.cpp:33-35,58-60,90-93`, `S/Vulkan/Shadow.ixx:26-31`, `S/Vulkan/Memory.ixx:200-209`
- **Problem:** 40 directional + 10 spot + 60 point layers of 1024² D32 (440 MiB VRAM) are allocated at init regardless of the scene, and `recordShadowMap` begins, clears, ends and double-barriers all 110 layers every frame even with one light. `prepareShadowFrame` packs every enabled light, so a light with intensity <= 0 still gets a shadow pass and PCF.
- **Change:** (1) Size the three `ShadowMap` arrays grow-only to the packed need (dir = lights·4, spot = lights, point = lights·6, minimum 1) in a new `ForwardRenderer::ensureShadowCapacity()` called from `drawFrame` before recording; a regrow waits idle (or retires, after R3) and rewrites the three shadow descriptors of both sets via `writeShadowArray`. (2) Give `VulkanImage::create` a flag that forces a `VK_IMAGE_VIEW_TYPE_2D_ARRAY` whole view, since `createView` makes a 2D view for 1 layer and the shader samples `Sampler2DArrayShadow`. (3) Transition every layer to `SHADER_READ_ONLY_OPTIMAL` once after creation (`submitOnce` + `transitionImage`) and loop `recordShadowMap` only over active layers. (4) In `prepareShadowFrame` skip point lights with intensity <= 0; spot and directional lights with intensity <= 0 stay packed only when ambient > 0, get no shadow pass, and carry a "no shadow" flag in the unused `w` of `spotLightDirections` / `directionalLightDirections`, which the shader checks before PCF.
- **Done when:** in `tests/SimpleForwardRendererTests.cpp`, after the existing 1 directional + 3 spot + 2 point frame, `dirShadowArray.layerCount == 4`, `spotShadowArray.layerCount == 3`, `pointShadowArray.layerCount == 12`. A new diagnostic `lastShadowLayerPassCount()` returns 19, and it stays 19 after adding a spot light with intensity 0. `hasDirectionalRuntimeToggleCoverage` (40 layer views after 10 lights) still passes. R14 reports no validation errors.
- **Risk / notes:** The first frame after a light is added can stutter because of the regrow.

### R3 — Replace vkDeviceWaitIdle in scene sync with per-frame updates and deferred destruction
- **Where:** `S/Render/RendererResources.cpp:306-364` (idle at :318-321), `:265-299` (descriptor rewrite :287-295), `:204-255`, `S/Render/RendererDraw.cpp:26,41`
- **Problem:** Any texture, material, topology or geometry change stalls the whole GPU with `vkDeviceWaitIdle`. `setObjectMeshPositions` does this every frame for animated meshes, and every `add*` does it because it marks materials dirty. `uploadSceneTextures` also rewrites all 64 texture descriptors of both sets for one new texture.
- **Change:** Call `syncSceneResources()` after the in-flight fence wait in `drawFrame`. Add a retire queue: replaced or removed `VulkanMesh`/`TextureImage`/`VulkanBuffer` objects are moved into it with the current submit serial and destroyed once the completed serial, advanced after each fence wait, reaches that tag. Give data that is updated in place one copy per frame slot, updated only while that slot is being recorded: the material buffer (set f binds buffer f) and the vertex buffers of meshes that received `updateVertices`, with per-slot dirty bitmasks. Write only changed texture array elements (`dstArrayElement = index`, count 1) into set f when frame f is recorded. After that, `vkDeviceWaitIdle` is needed only in `recreateSwapchain`, `cleanup` and debug readback.
- **Done when:** all renderer idle waits go through one private helper that increments `deviceWaitIdleCount()`. A new `tests/RenderRuntimeUpdateTests.cpp` (hidden window via `vve::simple::Windows`, registered with `vve_add_engine_test(... Vulkan::Vulkan)`) renders a first frame, then 5 frames each calling `setObjectMeshPositions`, 1 frame adding a textured cuboid and 1 frame removing an object. It asserts that the idle count is unchanged, `gpuMeshUploadCount()` rose by at least 5, `gpuTextureCount() == 1`, and R14 reports no errors.
- **Risk / notes:** This is the largest item. Per-slot copies double the memory of dynamic meshes and materials. Do it before R4 and R6.

### R4 — Use device-local memory for GPU-read buffers and pick VMA host-access flags per use
- **Where:** `S/Vulkan/Memory.ixx:80-96` (flags :85-87), `S/Vulkan/Resources.ixx:54,321,323,395`, `S/Render/RendererResources.cpp:234-235`, `S/Vulkan/Readback.ixx:44`
- **Problem:** Every host-visible buffer (vertex, index, UBO, material SSBO, staging, readback) is created with `HOST_ACCESS_RANDOM_BIT`, so VMA prefers host-cached system memory. On discrete GPUs, vertex, index, uniform and material data are then fetched over PCIe on every draw.
- **Change:** Replace `bool hostVisible` in `VulkanBuffer::create` with `enum class BufferMemory { device_local, upload, readback }`. `device_local` gets no host flags and requires `DEVICE_LOCAL`; `upload` gets `HOST_ACCESS_SEQUENTIAL_WRITE_BIT | MAPPED_BIT`; `readback` gets `HOST_ACCESS_RANDOM_BIT | MAPPED_BIT`. Keep HOST_COHERENT or call `vmaFlushAllocation`. Static vertex and index buffers become `device_local`, filled through an `upload` staging buffer and `vkCmdCopyBuffer` (batched with R6). UBOs, the material SSBO, dynamic per-frame vertex buffers (R3) and staging buffers use `upload`; `VulkanReadback` uses `readback`. Put the mapping in a pure helper `allocationInfoFor(BufferMemory)`.
- **Done when:** a new non-GPU test `tests/VulkanMemoryPolicyTests.cpp` asserts the helper's flags: `upload` has SEQUENTIAL_WRITE and no RANDOM, `readback` has RANDOM, `device_local` has no HOST_ACCESS bits. `SimpleForwardRendererTests` checks with `vmaGetAllocationMemoryProperties` that a static mesh's vertex buffer has `DEVICE_LOCAL_BIT`, and that the frame UBO has no `HOST_CACHED_BIT` unless it also has `DEVICE_LOCAL_BIT` (UMA).
- **Risk / notes:** Animated meshes must stay on `upload` memory, which depends on R3.

### R6 — Mipmapped textures, one shared sampler, batched uploads
- **Where:** `S/Vulkan/Memory.ixx:147-160` (`mipLevels = 1` at :152), `:200-209`, `S/Vulkan/Resources.ixx:48-87`, `S/Render/RendererResources.cpp:273-285`
- **Problem:** Material textures have a single mip level (`maxLod 1`, mipmap mode NEAREST), so minified textures alias and thrash the cache. Each texture creates its own identical `VkSampler`. Each texture is uploaded with its own staging buffer and a blocking `submitOnce`, i.e. one queue submit plus fence wait per texture when a scene loads.
- **Change:** Add a `mipLevels` parameter to `VulkanImage::create` and store it, with views covering all levels. `TextureImage::create` uses `1 + floor(log2(max(w,h)))` levels and `TRANSFER_SRC` usage, and generates the chain with per-level `vkCmdBlitImage(LINEAR)` and barriers. It falls back to 1 level if the format lacks `SAMPLED_IMAGE_FILTER_LINEAR`/`BLIT_SRC|DST`. Replace `TextureImage::textureSampler` with one `ForwardRenderer::materialSampler` (LINEAR/LINEAR/mip LINEAR, REPEAT, `maxLod = VK_LOD_CLAMP_NONE`, anisotropy when `samplerAnisotropy` is supported and enabled in `VulkanDevice::create`). Have `uploadSceneTextures` record all new textures into one command buffer through a small `VulkanUploadBatch` in `Memory.ixx` that owns the staging buffers until a single fence wait.
- **Done when:** in `SimpleForwardRendererTests::hasRuntimeGpuObjectSynchronization`, after adding the crate texture, `objectTextures[0].mipLevels == 1 + floor(log2(max(extent)))`. In `RenderTextureDedupTests`, adding two different textured cuboids before one frame raises a new `textureUploadSubmitCount()` by exactly 1. Capture tests still pass and R14 reports no errors.
- **Risk / notes:** Minified surfaces in captures get smoother, so capture thresholds need a re-check.

### R2 — Cull draws per pass and remove per-draw lookups and redundant binds
- **Where:** `S/Render/RendererDraw.cpp:173-193,227-231,248-255,319-321`
- **Problem:** Every visible instance is drawn in every active shadow layer and in the forward pass without frustum or light-range tests. Each draw does two `std::map::find` calls (`meshes`, `materialSlots_`) and rebinds VB/IB, and the shadow pipeline plus descriptor set are rebound inside every layer. CPU and GPU cost therefore scale with instances × (shadow layers + 1).
- **Change:** Build a per-frame `std::vector<DrawItem>` once in `recordCommandBuffer` with `const VulkanMesh*`, material slot, model, flags and world AABB (mesh bounds cached in `VulkanMesh` at `create`/`updateVertices` from `RenderMesh::bounds`; `valid == false` means never culled). Sort it by mesh and bind VB/IB only when the mesh changes. Cull against each pass's frustum: camera projection·view, spot view-proj, point-face view-proj (or the light sphere), directional cascade view-proj, which already contains the z backoff. Bind the shadow pipeline and set once before the first shadow layer, because bound state persists across `vkCmdBeginRendering`.
- **Done when:** a new diagnostic `lastFrameDrawStats()` {forwardDraws, shadowDraws, vertexBufferBinds, pipelineBinds} is added. `SimpleForwardRendererTests` asserts that a cuboid behind the camera adds 0 forward draws, a spot light with range 3 placed far from all objects adds 0 shadow draws, and `pipelineBinds == 2` in a frame with shadows. `RenderMeshDedupTests` (two instances of one mesh) asserts 1 vertex-buffer bind in the forward pass.

### T8 — Add the missing lifetime, input, resize and texture tests
- **Where:** new test files as listed below; existing partial coverage in `tests/RenderTextureDedupTests.cpp:134-146` and `tests/WindowInputTests.cpp:45-52`
- **Problem:** None of the following has a test:
  - resize / swapchain recreate;
  - `removeObject` followed by `removeSceneInstance`;
  - `clearScene` after `instantiateScene`;
  - removing imported lights and cameras with `removeSceneInstance`;
  - GUI input exclusivity;
  - a key tap within one poll;
  - that "hidden" windows really are hidden;
  - embedded glb textures;
  - HEIGHT-map greyscale rejection;
  - UV orientation.

  Texture slot reuse after purge is only partly covered: the index and GPU count are checked, but not the pixels.
- **Change:**
  - **Resize:** use `tests/RenderResizeTests.cpp` from R9 (do not create a second file). Also assert that the capture after resize has the new pixel size and that the post chain still applies.
  - **`tests/SceneInstanceLifetimeTests.cpp`** (facade, no GPU; OBJ with `o A`/`o B`, glTF with a point light and a camera):
    - (a) `removeObject(objs[0])` then `removeSceneInstance` succeeds; `objs[1]` and the instance give `missing_object`.
    - (b) Instantiate twice with `apply_lights`/`apply_cameras`: point lights and cameras 2/2, after removing one instance 1/1, then 0/0. A point light added with `addPointLight` beforehand survives.
    - (c) After `clearScene`, `removeSceneInstance`/`objectVisible` give `missing_object`; mesh, material, light and camera counts are 0; a re-instantiate gives the first mesh count again.
  - **`tests/WindowEventTests.cpp`** (white-box `simple::WindowSystem` + `GuiSystem` with a hidden window; link `SDL3::SDL3 imgui::imgui`), using `SDL_PushEvent` with `WindowSystem::poll`:
    - key down and key up in one poll: `wasKeyPressed && wasKeyReleased && !isKeyDown`;
    - a sink claiming KEY_DOWN: no press, and the later KEY_UP gives no release;
    - a key held before the sink claims everything: KEY_UP still releases it;
    - a claimed MOUSE_MOTION: zero delta;
    - `GuiSystem::processEvent` returns `io.WantCaptureKeyboard` for KEY_DOWN (set the flag by hand) and false for KEY_UP.
  - **`tests/RenderTextureOrientationTests.cpp`** (white-box, 64×64, unlit objects). Use a 2×2 texture with four colours (TL red, TR green, BL blue, BR white).
    - A `addTexturedCuboid` front face and a glTF quad whose PNG is embedded in a `.glb` the test writes (PNG bytes as a literal) must both show the quadrants upright and unmirrored in the capture.
    - Assert that `materialTextureSources` has `embedded != nullptr` and a path ending in `#*0`.
    - Assert `SDL_GetWindowFlags(native) & SDL_WINDOW_HIDDEN`.
  - **`tests/RenderHeightMapTests.cpp`** (`simple::Engine` without windows):
    - `map_bump` with a grey PPM: `normal_texture_index == kNoRenderTexture`, and no texture slot is used for it.
    - `map_bump` with a coloured PPM: a normal index that is linear.
    - `map_bump` plus `map_Kn`: the `map_Kn` texture wins.
  - **RenderTextureDedupTests:** after the reuse at :140, add a solid-green texture, hide the other objects, capture, and assert that the centre pixel is green.
- **Done when:** All new tests are registered in `tests/CMakeLists.txt` and pass on the three platforms. Negative checks:
  - Removing the `keys_pressed_` insert for taps, or flipping the decoded image rows in `decodeTexture`, makes the respective test fail.
  - Skipping `removeImportedLights` in `rollbackSceneInstance` makes the respective test fail.
- **Risk / notes:** The GPU tests need a video driver; see T9.

## Phase 4 — Duplicated data and data model

### D3 — Fill FrameUniforms directly and slim ShadowLightMeta
- **Where:** `src/versions/simple/Render/Renderer.ixx:31-43,63-80`, `src/versions/simple/Render/RendererShadowPrep.cpp:22-151`, `src/versions/simple/Render/RendererDraw.cpp:49-68,253-255`, `src/versions/simple/Render/RendererDebug.cpp:28-42`, `src/versions/simple/Vulkan/Resources.ixx:362-383`, `src/versions/simple/shaders/simple_forward.slang:43-46,262-265`
- **Problem:** `ForwardRendererShadowFrame` repeats 13 of the 17 `FrameUniforms` members. About 8.5 KB of it is copied member by member into `FrameUniforms` every frame. `activePointLightCount` is computed but never uploaded; the shader finds live point slots by `range > 0 && intensity > 0` instead. `ShadowLightMeta` is rebuilt every frame with fields that carry no information: `shadow_slot == light_index`, `layer_count == 1`, and `resolution` is constant. Its `depth_bias = 0.001` is not the bias the shader uses. `first_layer` is a shadow-matrix index for point rows (+`kShadowMatrixPointBase`) but an array layer for spot and directional rows.
- **Change:** Make `prepareShadowFrame` fill a `FrameUniforms` member of ForwardRenderer (`frameUniforms_`, with a const accessor `frameUniforms()`) and delete `ForwardRendererShadowFrame`. Change `recordCommandBuffer`, `recordShadowDepthSamples` and RendererDebug to read the counts from it. Replace `FrameUniforms::padding` (C++ and Slang) with `activePointLightCount`; the std140 size stays identical. The point loop in `fragmentMain` then tests `i < activePointLightCount`. Remove `shadow_slot`, `layer_count`, `resolution` and `depth_bias` from `ShadowLightMeta`. `first_layer` becomes the layer inside that light type's own array (point: `packed*6+face`). `shadowCompareBias` becomes a named constant.
- **Done when:** The `static_assert(sizeof(FrameUniforms) == ...)` is unchanged. `hasPointShadowMetaInvariants` in SimpleForwardRendererTests expects `first_layer == light_index*6 + face` (the "> max spot layer" rule is dropped). A new check after the `point_shadow_scene` frame asserts `forward().frameUniforms().activePointLightCount == 2` and `activeSpotLightCount == 3`, since the disabled spot light is not packed. RenderLightingCaptureTests and RenderSponzaCaptureTests still pass.
- **Risk / notes:** This is a shader change, so run all capture tests. Do it before D9, because it removes two of D9's literals, and before D11, which uses the `frameUniforms()` accessor.

### D9 — Move the remaining shadow constants into simple_shared.h
- **Where:** `src/versions/simple/shaders/simple_shared.h:1-8`, `src/versions/simple/Scene.ixx:25-28`, `src/versions/simple/shaders/simple_forward.slang:22-25,133,279,288,315,319,347`, `src/versions/simple/Render/Renderer.ixx:88-91`, `src/versions/simple/Render/RendererShadowPrep.cpp:123,140,142`, `src/versions/simple/Render/RendererResources.cpp:105`, `tests/SimpleForwardRendererTests.cpp:106,191,212,690`
- **Problem:** These values are still typed by hand in C++ and Slang:

  | Value | Places |
  |---|---|
  | shadow near plane 0.1 | Renderer.ixx:89, ShadowPrep:123 and :140, slang:133 |
  | shadow floor 0.35 | Renderer.ixx:90, slang:288/319/347 |
  | directional bias 0.00005 | Renderer.ixx:91, ShadowPrep:142, slang:315 |
  | 6 cube faces | Scene.ixx:27, Renderer.ixx:88, RendererResources.cpp:105, slang:24 and :279 |
  | shadow-matrix base formulas | Scene.ixx:25-28 and slang:22-25 |

  Tests repeat them as literals. Editing one side silently desynchronises the CPU depth diagnostics from the shader.
- **Change:** Add `VVE_POINT_SHADOW_FACES`, `VVE_SHADOW_NEAR_PLANE`, `VVE_SHADOW_OCCLUDED_FACTOR` and `VVE_DIRECTIONAL_SHADOW_BIAS` (float literals with an `f` suffix, valid in both languages) and `VVE_SHADOW_MATRIX_POINT_BASE`/`_DIR_BASE`/`_COUNT` to simple_shared.h. Define `kShadowMatrix*`, `ForwardRenderer::pointShadowFaceCount/shadowNearPlane/occludedShadowFactor/directionalCompareBias` and the Slang statics from them, and use them at every listed literal, including the directional ortho near plane. Tests use the `vve::simple` constants.
- **Done when:** `grep -nE "0\.35|0\.00005|\* ?6U?\b|[(= ]0\.1[);]" src/versions/simple/shaders/simple_forward.slang src/versions/simple/Render/RendererShadowPrep.cpp src/versions/simple/Render/RendererResources.cpp` is empty, and the same values appear only in simple_shared.h. SimpleForwardRendererTests (CPU/GPU depth agreement, occluded shadow factor, directional bias) and the capture tests pass.
- **Risk / notes:** Do after D3. The camera near/far literals in RendererDraw.cpp:46-47 belong to D11.

### D1 — Store each light once, in the renderer's light list
- **Where:** `src/versions/simple/Scene.ixx:41-82`, `src/Types.ixx:87-128`, `src/versions/simple/Render/RenderResources.ixx:72-102,177-181,400-467`, `src/versions/simple/Render/RenderSystemScene.cpp:77-280`, `src/versions/simple/Render/RenderSystemObjects.cpp:261-278`, `src/versions/simple/Render/RenderSystem.ixx:327-331,352-354`, `tests/SimpleForwardRendererTests.cpp:552-583`
- **Problem:** Every light setter writes to `renderer_.scene` (the list that gets rendered) and again to RenderScene's `directional_lights_`/`point_lights_`/`spot_lights_` plus the `light_`/`point_light_` optionals. The CPU copies are read only by `scene*LightCount()`/`hasScene*Light()` and one unit test, but they still need their own `owner` tags, their own `eraseImportedBy` path, and the mirroring loop in `loadScene`. The four light shapes have different defaults. For a point light, intensity is 3/1/0, range is 7/10/1 and ambient is 0.18/0.04/0.04. For example, the 4-arg `addPointLight` gives the backend an ambient of 0.18 but the CPU copy 0.04. `simple::PointLight`/`SpotLight`/`DirectionalLight` also hide the facade `vve::` names inside `vve::simple`.
- **Change:** Delete `RenderDirectionalLight`/`RenderPointLight`/`RenderSpotLight`, RenderScene's light members, `set*/add*Light`, `clearLights` and the light part of `eraseImportedBy`. Remove every `scene_.add*/set*Light` call in RenderSystemScene.cpp and the mirroring loop in `RenderSystem::loadScene`. Implement `scene*LightCount()` as `renderer_.scene.<list>.size()` and `hasScene*Light()` as `!empty()`. Rename the backend structs to `ForwardPointLight`/`ForwardDirectionalLight`/`ForwardSpotLight` so the facade names become visible again, and give `Scene::ambient` its own literal instead of `PointLight{}.ambient`. The ambient-less overloads (4-arg set/addPointLight, 6-arg set/addSpotLight) forward to the explicit overloads with `vve::PointLight{}.ambient`/`vve::SpotLight{}.ambient`, so both API spellings give the same light. `RenderSystem::loadSampleScene` (src/RenderSystem.cpp:32-41) passes its ambient explicitly.
- **Done when:** The RenderScene block in SimpleForwardRendererTests `main()` is rewritten against `render_system.forward().scene.directionalLights`: cap at `kMaxDirectionalLights`, the first light is kept, `set` replaces all. A new check there asserts that `addPointLight(p,c,i,r)` and `addPointLight(p,c,i,r, vve::PointLight{}.ambient)` produce equal `ambient` in `forward().scene.pointLights[0/1]`. RenderLightCountTests and RenderSceneLightApplyTests pass unchanged, including removal by scene instance. `grep -rn "RenderPointLight\|point_light_\b\|light_view_projection" src tests` is empty.
- **Risk / notes:** Callers that relied on "omitted ambient keeps the current one" (4-arg `setPointLight`, 6-arg `setSpotLight`) now get the descriptor default. Only loadSampleScene and tests do this. Do this before D11, which edits the same RenderScene members.

### E2 — Let testscene switch lights off instead of adding zero-intensity lights
- **Where:** `examples/testscene/testscene.cpp:276-317` (`applyLights`); `src/RenderSystem.ixx` (no light-removal call); `src/versions/simple/Render/RendererShadowPrep.cpp:32-34,58-59,92` (only the backend `enabled` flag skips a light)
- **Problem:** A directional or spot light switched off in the GUI is still added, with intensity 0 (and range 0). Facade lights are always `enabled`, so each still gets its shadow pass: one per spot light, four cascades per directional light. Toggling lights off to compare cost changes nothing. The only way to drop lights through the facade is `clearScene`, which also drops the objects.
- **Change:** Add `RenderSystem::clearLights()` to the facade and to simple. It clears the three light lists in `RenderScene` and the renderer, and keeps objects and imported-light owners consistent. Have `applyLights` call it and then add only the enabled lights; this also removes the point-light fallback at :314-316.
- **Done when:** `tests/RenderLightCountTests.cpp` asserts that after adding one light of each kind plus a plane, `clearLights()` makes all three counts 0 while `sceneInstanceCount()` stays 1. In `testscene` with every spot light switched off, the spot-shadow pass count in the frame is 0; check this once with the white-box `lastRecordedPassOrder`.
- **Risk / notes:** Do this after D1, which restructures the light storage.

### D6 — Keep one texture identity and drop unresolvable TextureHandles
- **Where:** `src/versions/simple/Render/RenderResources.ixx:33,50-51`, `src/versions/simple/Render/RenderSceneImport.cpp:210-216`, `src/versions/simple/Render/RenderSystemObjects.cpp:222-225`, `src/versions/simple/Assets.ixx:66,92,147,153,190,197-199,470-479,518`, `src/Assets.ixx:29,36,62`, `src/Assets.cpp:53-54,88-89,195-196`, `src/Types.ixx:23,34`
- **Problem:** RenderMaterial carries a newly minted `base_color_texture` TextureHandle that nothing reads. Its `base_color_texture_source` path duplicates `RenderTexture::canonical_path`, and the `texture_indices_` key holds the same path a third time. `Material::textures`, `AssetScene::textures` and the facade `materialTextures()`/`sceneTextures()` hand out TextureHandles that no API can resolve. `kNoRenderTexture` repeats `kNoTexture` (both 0xFFFFFFFF).
- **Change:**
  - Remove `RenderMaterial::base_color_texture` and `::base_color_texture_source` and their writers. Use `kNoTexture` everywhere instead of `kNoRenderTexture`.
  - Remove `Material::textures`, `AssetScene::textures`, `MaterialImport::textures`, `AssetSystem::texture()`, `materialTextures()` and `sceneTextures()` (simple and facade), and then `TextureHandle`/`TextureHandleTag`, which have no users left.
  - Reimplement `sceneTextureCount()` as the number of distinct `MaterialTextureSource::path` values over the scene's materials.
  - Keep `texture_indices_` as the lookup index: `canonical_path` is needed to erase its key.
- **Done when:**
  - `grep -rn "kNoRenderTexture\|base_color_texture_source\|materialTextures\|TextureHandle" src tests examples` is empty.
  - RenderMaterialImportTests replaces the `is_absolute()` checks with `render.sceneTextureIsLinear(m.base_color_texture_index) == false` and `...(m.normal_texture_index) == true`, replaces `materialTextures()` with `sources->size() == 2`, and asserts `assets.sceneTextureCount(*scene) == unique_paths.size()`.
- **Risk / notes:** This is a public facade API removal (`materialTextures`, `sceneTextures`); update examples/FACADE_AUDIT.md.

### D7 — Derive duplicated asset data; drop unread render bounds and graph labels
- **Where:** `src/versions/simple/Assets.ixx:39,47-48,89,177,179-180,461-467,585-586,604-606,613`, `src/versions/simple/Render/RenderResources.ixx:40,118-121,314-333,342-345,370-371`, `src/versions/simple/Render/RenderSceneImport.cpp:152-164`, `src/versions/simple/Render/RenderSystemObjects.cpp:148-163,189,195-208`, `src/versions/simple/Graph.ixx:13,24,36-37,49-55,84-103`
- **Problem:** `RenderMesh::bounds` is computed in five places (plane, cuboid, triangle mesh, imported mesh, `setObjectMeshPositions`) and never read, because there is no culling. `AssetMesh::bounds` (what `meshBounds()` returns) is computed separately at import. `Node::materials`, `AssetMesh::vertex_count/index_count` and `AssetScene::nodes` repeat data already held in `AssetMesh::material`, the array sizes and the scene tree. Graph stores an `ObjectName` per node that no caller sets or reads.
- **Change:**
  - Remove `RenderMesh::bounds`, the `Bounds` parameters of `RenderScene::addMesh`/`addTriangleMesh`, and all min/max loops in RenderSystem. Keep `AssetMesh::bounds`, because the facade exposes it.
  - Remove `Node::materials`, `AssetMesh::vertex_count/index_count` and `AssetScene::nodes`, and derive the facade queries instead:
    - `nodeMaterials()` from `nodeMeshes()` + `meshMaterial`.
    - `meshVertexCount()`/`meshIndexCount()` from the array sizes.
    - `sceneNodes()` from a new `Graph::nodes()`. Handle order is creation order, which is pre-order.
  - `sceneWithNode` uses `tree.contains()`.
  - Turn `Graph::nodes_` into `std::set<THandle>` and drop the `ObjectName` parameters of `addNode`/`setRoot`/`addChild`.
- **Done when:** SceneSystemTests, SceneInstantiationTests and SceneTransformCompositionTests pass unchanged. SceneSystemTests additionally asserts that `nodeMaterials(n)` equals `meshMaterial(m)` for each `m` in `nodeMeshes(n)`, and that `sceneNodes(scene)->front() == *sceneRootNode(scene)`. `grep -rn "bounds" src/versions/simple/Render` is empty.
- **Risk / notes:** When frustum culling (#56) lands, compute bounds once in `RenderScene::addMesh` from the vertices. D14's `sceneWithNode` fix is included here.

### D11 — Remove write-only fields and the unused camera plumbing
- **Decision DEC-4** (see the table above).
- **Where:** `src/versions/simple/Render/RenderResources.ixx:104-109,176,182,386-398`, `src/versions/simple/Render/RenderSystemScene.cpp:70-75`, `src/versions/simple/Render/RendererDraw.cpp:46-47`, `src/Types.ixx:147-150,264-279`, `src/versions/simple/Render/RenderSystem.ixx:133,187,197,252-254,270`, `src/versions/simple/Render/Renderer.ixx:168,269`, `src/versions/simple/Engine.ixx:61,129,151-152,230`, `src/versions/simple/Vulkan/Resources.ixx:147,167,295`, `src/versions/simple/Vulkan/Commands.ixx:71`, `src/versions/simple/Vulkan/Memory.ixx:131,168,195`, `src/versions/simple/Vulkan/Device.ixx:38,65-68`, `src/versions/simple/Window.ixx:48,91,107,132-139,146,222-236,507-544`, `src/Window.ixx:195,223-230`, `src/Window.cpp:82,134-169`, `src/Error.ixx:35,55`
- **Problem:** The `fov_y` part of the review is fixed: fov now reaches the projection. The rest is still open:

  | Kind | Items |
  |---|---|
  | Written, never read | `Camera::view_transform`, `Camera::clip` (drawFrame hard-codes near 0.1/far 100), `RenderCamera::target_extent` and the stored `RenderCamera` itself (only `has_value()` is read), `RenderSystem::active_scene_` (new), both `guiSystem_` pointers, `VulkanDescriptorSets::descriptorPool`, `VulkanImage::layerCount`, `simple::Engine::last_frame_time_`, `simple::Window::sdl_id_` (new; lookups use `Impl::indices`) |
  | Read for its size only | `imported_cameras_` |
  | Member that is only a local | `VulkanInstance::validationEnabled` |
  | Duplicate storage | raw command-buffer vector beside the owned one |
  | Never produced | `Error::cycle_detected` (new) |
- **Change:**
  - Pass `camera.clip` to `ForwardRenderer::setCamera` and use it in `drawFrame`. Set the `ClipPlanes` default `far_plane` to 100 so default scenes keep today's depth range.
  - Delete `Camera::view_transform`.
  - Replace `RenderScene::camera_` with a bool and `imported_cameras_` with a vector of owner tags; that keeps the count and per-instance removal. Delete `RenderCamera`. (`setCamera`'s `PixelExtent` parameter is removed in W3c.)
  - Delete the other listed fields and `setGuiSystem`. Make `validationEnabled` a local. Submit and record via the owned command buffers.
  - Remove every `Error` value nothing produces (today `cycle_detected`, `already_initialized`, `file_not_found`, `unsupported_version`; re-check with grep) and their `errorName` entries.
- **Done when:**
  - Each removed name greps empty in src/.
  - SimpleForwardRendererTests asserts that after `setCamera` with `ClipPlanes{0.5, 50}` the uploaded `frameUniforms().projection` equals `perspectiveVulkan(fov, aspect, 0.5, 50)`, using the accessor from D3.
  - RenderCameraCountTests and RenderSceneCameraApplyTests pass, including camera removal with the scene instance.
- **Risk / notes:**
  - The window-camera API and the `setCamera` signature are handled by W3c; do this task after W3c.
  - Removing `Error` values is a public API change; update `errorName` and any documentation that lists them.
  - Keep the ECS: it is the user entity store exposed through World (ECSTests), not engine state.
  - Do this after D1 (same RenderScene members) and D3.

### D12 — Replace the segmented Vector with std::vector
- **Decision DEC-5** (see the table above).
- **Where:** `src/Vector.ixx:18-489`, `src/Assets.cpp:7-13`, `src/versions/simple/Vulkan/Resources.ixx:346-357`, `tests/SegmentedVectorTests.cpp`, `tests/CMakeLists.txt:8`
- **Problem:** `vve::Vector` allocates whole 256-element segments, so every non-empty Vector costs at least 256·sizeof(T): 2 KB for one MeshHandle, about 35 KB for a one-window snapshot. It has no `data()`, so GPU uploads copy element by element (`uploadValues`) and nothing can be viewed as a span. Node lists, mesh geometry, per-frame window snapshots and every facade query result use it.
- **Change:** Reduce VVEngine.Vector to `template <typename T> using Vector = std::vector<T>;`, which keeps `vve::Vector<T>` source-compatible for users and examples. Drop `implementation_type` and simplify `facadeVector`. Make `uploadValues` memcpy via `data()`. Audit code that relied on element addresses surviving `push_back`/`insert`, for example RenderScene `find*` pointers held across `add*` in `setObjectMeshPositions`.
- **Done when:** `tests/SegmentedVectorTests.cpp` is replaced by `tests/VectorTests.cpp` (registered in tests/CMakeLists.txt). It asserts that a `Vector<vve::MeshHandle>` holding one element has `capacity() < 256`, and that `v.data() + 1 == &v[1]` after two `push_back`s. Full ctest passes; on Linux, also run it once with `-fsanitize=address` to catch invalidated element pointers.
- **Risk / notes:** Losing pointer stability is the only behavioural difference. D13 (spans) and D14 (range erase) build on this.

### D5 — Share primitive meshes and materials and release them on removal
- **Where:** `src/versions/simple/Render/RenderSystemObjects.cpp:20-35,113-133,159-166,214-231`, `src/versions/simple/Render/RenderSceneImport.cpp:185-189`, `src/versions/simple/Render/RenderResources.ixx:498-518`, `examples/game/game.cpp:124-133,255-257`
- **Problem:** Every `addPlane`/`addCuboid`/`addTriangleMesh`/`addTexturedCuboid` call adds a new RenderMaterial and RenderMesh:
  - game.cpp's 100 grass tiles create 100 identical materials, and 100 meshes because each tile bakes its position into min/max.
  - Every spawned crate adds another mesh and material, and `removeObject` never frees them. No example calls `purgeUnusedAssets`, so the game leaks CPU and GPU memory, and each spawn triggers a material-buffer rebuild with `vkDeviceWaitIdle`.
  - `acquireRenderMaterial` adds a fresh default material for every mesh with an invalid material, on every instantiation.
- **Change:**
  - Add `primitive_materials_` to RenderSystem, keyed by (base color, texture index), and `primitive_meshes_`, keyed by (shape kind, extents) for plane and cuboid. Reuse an entry while `scene_.findMesh`/`findMaterial` still finds it.
  - `removeObject` erases the removed instance's mesh and material when no other instance references them and they are not in `imported_render_meshes_`/`imported_render_materials_`. It drops their cache entries and calls the texture release (make `releaseUnusedTextures` callable).
  - Cache one `default_material_` handle for invalid imported materials.
  - game.cpp builds each grass tile from one tile-sized cuboid placed by `Transform`.
- **Done when:** New `tests/RenderPrimitiveSharingTests.cpp` (facade, registered with `vve_add_engine_test`, no window needed) checks:
  - 100 `addTexturedCuboid` calls with the same texture and extents but different transforms give `sceneMaterialCount()==1`, `sceneMeshCount()==1` and `sceneTextureCount()==1`.
  - Removing every object leaves meshes, materials and textures at 0 without calling `purgeUnusedAssets()`.
  - White-box: a `vve::simple::RenderSystem` built with `ImportedAssetReadAccess` stubs whose `mesh_material` returns `MaterialHandle{}` for 3 meshes, instantiated twice, has `sceneMaterialCount()==1`.
- **Risk / notes:** `removeObject` now marks materials dirty, which costs a device wait (#57). Coordinate with D14, which rewrites purge.

### D14 — Make texture acquisition, lookups and purge sub-quadratic
- **Where:** `src/versions/simple/Render/RenderResources.ixx:188-200,256-288,478-496,520-553`, `src/versions/simple/Render/RenderSceneImport.cpp:82-99`, `src/versions/simple/Assets.ixx:461-467`
- **Problem:**
  - `acquireTexture` calls `weakly_canonical` (filesystem syscalls) before the `texture_indices_` lookup, even though imported paths are already canonical.
  - A failed decode is not remembered, so every material that references a missing or broken file reads and decodes it again. A cached height-map hit rescans the whole image in `isGreyscale`.
  - `RenderScene::findMesh/findMaterial/findInstance` are linear scans behind every per-object setter, so updating every object's transform each frame (the game's crates) costs O(n²) per frame.
  - `purgeUnusedAssets` scans all instances for every mesh and material, O(assets × instances), and then erases the tail one element at a time.
  - The import traversal does two linear `find`s per node, and `sceneWithNode` does a linear `contains` per `sceneNodeChildren` call, which makes the traversal O(n²) in the node count.
- **Change:**
  - In `acquireTexture`, look up the path as given first and canonicalize only on a miss.
  - Cache decode failures per key in a map that `clear()` empties.
  - Store the greyscale flag at decode (shared with D4).
  - Add handle→index `unordered_map`s for `meshes_`/`materials_`/`instances_`, maintained by `add*`, erase and purge.
  - `purgeUnusedAssets` collects referenced handles into `unordered_set`s in one pass and removes with one `remove_if` plus a range erase.
  - `importedSceneWorldTransforms` uses a visited `unordered_set` and a node set built once. `sceneWithNode` uses `tree.contains()` (see D7).
- **Done when:** RenderTextureDedupTests uses a new diagnostic `RenderSystem::textureDecodeCount()` and asserts:
  - Three `addTexturedCuboid` calls with the same missing file each return `io_error` and raise the count by exactly 1.
  - Two materials sharing one coloured height map decode it once.

  ScenePurgeUnusedAssetsTests and SceneTransformCompositionTests pass.
- **Risk / notes:** A cached failure is not retried until `clearScene()`, so a file fixed at runtime is picked up only after that. Do this after D12 (range erase) and together with D5, which also changes removal and purge.

### D13 — Stop copying asset data on every query
- **Where:** `src/versions/simple/Assets.ixx:150-161,172-192,441-460,522,587`, `src/versions/simple/Render/RenderSceneImport.cpp:123-138,171-173`, `src/versions/simple/Render/RenderSystem.ixx:67-71`
- **Problem:** `field()` and `sceneField()` return the member by value, so every mesh query copies its array. `importedMeshGeometry` copies all five arrays again into a tuple, and `acquireRenderMesh` copies the indices a third time. `sceneRootNode` copies the whole SceneTree (a map and two multimaps) just to read `root`. The `scene*Count` getters copy a vector to call `size()`. `materials()` and `meshes()` insert with `add(item)`, which copies the descriptor with all of its geometry.
- **Change:**
  - `field()`/`sceneField()` return `std::expected<const T *, Error>`. The count getters and `sceneRootNode` read through the pointer, and public by-value getters copy once at the end.
  - Give `ImportedAssetReadAccess` span-returning mesh callbacks (`std::span<const Vec3>` and so on) plus a matching `simple::AssetSystem::mesh*View()`. `acquireRenderMesh` builds vertices and indices straight from the spans.
  - In `materials()` and `meshes()`, save the handle and call `add(std::move(item))`.
- **Done when:** RenderMeshDedupTests (white-box) asserts that two calls to `engine.assets().meshPositionsView(mesh)` return spans with the same `data()`, which proves no copy is made. Scene*Tests and RenderMaterialImportTests pass. `grep -n "\.add(item)" src/versions/simple/Assets.ixx` is empty.
- **Risk / notes:** Depends on D12, because spans need contiguous storage. If D4's shared geometry lands first, the mesh part of this item is already covered.

### D4 — Share imported geometry and free decoded pixels after upload
- **Where:** `src/versions/simple/Assets.ixx:43-56,556-587`, `src/versions/simple/Render/RenderResources.ixx:36-41,264-268`, `src/versions/simple/Render/RenderSceneImport.cpp:123-177`, `src/versions/simple/Scene.ixx:33-39`, `src/versions/simple/Render/RendererResources.cpp:278-284`
- **Problem:** Imported geometry is stored three times: the AssetMesh SoA arrays (engine lifetime), an interleaved RenderMesh copy, and the GPU buffers. `RenderTexture::rgba8` keeps every decoded image in RAM after upload, roughly 64 MB per 4K texture. It is kept because `uploadSceneTextures` and the height-map check on cache hits (`isGreyscale`) read it. The review's "past the cap" part is fixed: `acquireTexture` now returns `capacity_exceeded` before it decodes.
- **Change:** Textures:
  - Add `ForwardRenderer::uploadedTextureGeneration(i)`. After `renderer_.renderFrame`, RenderSystem calls a new `RenderScene::releaseUploadedPixels(...)` that clears (`std::vector{}.swap`) the pixels of every slot whose generation is resident.
  - Compute a `greyscale` flag once at decode, for the height check.
  - Keep the source (canonical path or `shared_ptr<const EmbeddedImage>`) in RenderTexture so that `uploadSceneTextures` can re-decode a slot with empty pixels after a renderer re-init.

  Geometry (preferred):
  - AssetMesh stores one `std::shared_ptr<const std::vector<RenderVertex>>` plus indices, built at import. The facade `meshPositions/Normals/Texcoords/Tangents` extract from it.
  - A RenderMesh made from an imported mesh holds the same pointer, obtained through a new `ImportedAssetReadAccess::mesh_geometry` callback.
  - `setObjectMeshPositions` copies before writing when `use_count() > 1`; it already clones shared meshes.

  Freeing RenderMesh CPU data after upload was rejected because `setObjectMeshPositions` and renderer init read it.
- **Done when:**
  - RenderTextureDedupTests uses a new diagnostic `RenderSystem::sceneTexturePixelBytes()`. It asserts the value is > 0 after `addTexturedCuboid` and before the first `renderFrame`, and == 0 after that frame while `gpuTextureCount()` is unchanged.
  - RenderMaterialImportTests still passes, which covers the height rejection on a cache hit.
  - RenderMeshDedupTests (white-box) asserts that the RenderMesh of an instantiated imported mesh shares its vertex buffer with the asset catalog (`use_count() == 2`, same `data()`).
- **Risk / notes:** Re-decoding after a renderer re-init needs the file to still exist. This overlaps D13 (the mesh copies disappear) and D6 (RenderTexture fields). If spans are used, do D12 first.

### D8 — Define the window, frame-info and shadow-sample structs once
- **Where:** `src/Window.ixx:15-77`, `src/Engine.ixx:22-54,87,243-248,293-321,405-414`, `src/Engine.cpp:15-53,96-100`, `src/versions/simple/Window.ixx:24-57`, `src/RenderSystem.ixx:18-34`, `src/RenderSystem.cpp:253-270`, `src/versions/simple/Render/Renderer.ixx:45-61`, `src/Types.ixx:7,15-16,178-198`, `src/ECSContainer.ixx:3,13-14`, `src/versions/simple/Engine.ixx:28,82-85,190-193`
- **Problem:**
  - The eight window-creation fields exist three times (`WindowSetup`, `detail::EngineWindowSetup`, `simple::WindowDesc`) and are converted twice at startup.
  - `WindowFrameInfo` is identical to `simple::WindowInfo`. `engineWindowFrame` copies the snapshot, strings included, into it every frame, even for an `Engine<>` without systems.
  - `RenderShadowDepthSample` exists twice with different defaults (shadow_factor/gpu_depth/error 0/0/0 vs 1/-1/-1) and is copied field by field.
  - `Entity` is re-declared with `using` in VVEngine.Types and VVEngine.ECSContainer.
  - `EngineConfig` duplicates `ApplicationName`+`MaxFrames` and has no user in tests or examples.
- **Change:**
  - Define `WindowDesc`, `WindowInfo`/`WindowFrameData` and `RenderShadowDepthSample` (simple's defaults) once, as plain data in VVEngine.Types (namespace vve), and have simple use them.
  - `WindowSetup` wraps a `WindowDesc`. Delete `EngineWindowSetup`, `WindowFrameInfo`, `implementationWindows`, `facadeWindowFrame` and the field-by-field sample copy.
  - Use `export import VVEngine.Entity;` in Types.ixx/ECSContainer.ixx instead of the using-declarations.
  - Delete `EngineConfig` and both `Engine(EngineConfig)` constructors.
  - Skip `engineWindowFrame` in `updateSystems` when `sizeof...(TSystems) == 0`.
  - Frame-counter duplication is out of scope here.
- **Done when:** `grep -rn "EngineWindowSetup\|WindowFrameInfo\|EngineConfig\|facadeWindowFrame\|using vve::Entity" src` is empty. WindowOwnershipTests, WorldTests and SimpleForwardRendererTests pass. UserSystemTests asserts that the window frame a system receives carries the configured window's id, title and extent.
- **Risk / notes:** Removing `EngineConfig` changes the public API; update AGENTS.md:55, src/versions/simple/AGENTS.md:36 and examples/FACADE_AUDIT.md:19. Do this with D10 or before it, since both rewrite Engine.cpp. If D11 lands, the shared `WindowInfo` has no `camera`.

### D10 — Drop the EngineState pimpl
- **Decision DEC-6** (see the table above).
- **Where:** `src/Engine.ixx:3,56-74,120-121,237-241,268-275,363-396,409`, `src/Engine.cpp:8-13,55-105`
- **Problem:** `vve::Engine` reaches `simple::Engine` through an opaque `detail::EngineState`, a custom deleter, `makeEngineState` and 11 forwarding functions. Yet `export import :Implementation` already makes `detail::EngineImpl` (= `simple::Engine`) and every simple type visible to importers. The indirection hides nothing and costs a heap allocation plus a call layer per frame hook.
- **Change:** Replace `detail::EngineStateHandle state_` with `std::unique_ptr<detail::EngineImpl> impl_`, created by one non-template factory in Engine.cpp that keeps the option conversion out of the header. The `Engine` template then calls `impl_->assets()`, `renderSystem()`, `gui()`, `windowSystem()`, `ecs()`, `init()`, `step()` and `renderFrame()` directly. Delete `EngineState`, `EngineStateDeleter` and the 11 accessors. This is preferred over un-exporting `:Implementation`, because C++ requires every interface partition to be exported by the primary interface ([module.unit]/3), so the simple types cannot be hidden that way.
- **Done when:** `grep -nE "EngineState|engine[A-Z][A-Za-z]*\(" src/Engine.ixx src/Engine.cpp` is empty. WorldTests, UserSystemTests and WindowOwnershipTests pass, and all examples build.
- **Risk / notes:** AGENTS.md:55 describes the "opaque state handle"; update it. Do after D8, or together with it (D8 removes the conversion helpers).

### W6 — Stop copying the window snapshot two to three times per frame
- **Where:** `src/Engine.cpp:37-53` (`facadeWindowFrame`), `:97-100` (`engineWindowFrame`); `src/Engine.ixx:406-414` (`updateSystems`); `src/versions/simple/Render/RenderSystem.ixx:357-360, 380-382`; `src/versions/simple/Window.ixx:467-472` (`snapshot`)
- **Problem:** Each facade `step()` makes three copies:
  1. `snapshot()` copies every `WindowInfo` (two strings each) into a `Vector` with a 256-element segment (about 35 KB).
  2. `facadeWindowFrame` copies it again into `WindowFrameInfo`.
  3. `RenderSystem::renderFrame(WindowSystem&)` takes another snapshot, only to count the windows that are not closing.

  This happens even for `Engine<>` without user systems, because `systems_` is then an engaged empty tuple.
- **Change:**
  - `detail::engineWindowFrame` builds `WindowFrameInfo` directly from `state.impl.windowSystem().windows()` (one copy). Then delete `simple::WindowSystem::snapshot()` and the `WindowFrameDataImpl` alias.
  - Facade `updateSystems` builds the frame data only when some `TSystems` has the 3-argument `update`: a constexpr `requires` check with the same expression as the `Priority<3>` overload of `invokeUserSystemUpdate`.
  - `RenderSystem::renderFrame(WindowSystem&)` stops taking a snapshot. With W3b and W8 the count is "presented windows" and needs no window data; remove the `renderFrame(const WindowFrameData&)` overload.
- **Done when:** `grep -rn "snapshot()" src` finds nothing. `WindowOwnershipTests` (2 windows, per-window cameras, `window_count == 2`) and `UserSystemTests` (`last_window_count == 1`) pass unchanged.

## Phase 5 — Build and infrastructure

### B7 — Make build_linux.sh portable and stop wiping the build dir on every run
- **Where:** `build_linux.sh:6-7` (hard-coded `/home/hlavacs/vcpkg/downloads/tools/cmake-4.3.3...`), `:73-83` (clang-18 only; hard-coded import-std UUID `451f2fe2...`, the CMake 4.3 one), `:84-99` (cache checks); `triplets/x64-linux-llvm.cmake:6`; `cmake/toolchains/linux-x64-llvm-libcxx.cmake:2-3`; `cmake/CMakePresets.base.json:29-41`
- **Problem:** Without `/usr/bin/clang++-18`, the check `grep "^CMAKE_CXX_COMPILER:.*clang++-18$"` at `:93` never matches. Every run then does `rm -rf build/<variant>-linux`: a full rebuild plus a fresh vvppl download. The script never runs `vcpkg install`, unlike the Windows and macOS scripts. The overlay triplet is only found with `--overlay-triplets=triplets`, and there is no `vcpkg-configuration.json`. The triplet toolchain builds dependencies with a hard-coded clang-18, whatever compiler the engine uses. The UUID and `CMAKE_CXX_MODULE_STD` passed with `-D` are overridden by `CMakeLists.txt:15-16` anyway.
- **Change:**
  - Take cmake/ctest from `${CMAKE:-$(command -v cmake)}`.
  - Select LLVM through `VVE_LLVM_VERSION`: the env value, else the newest `clang++-NN` that also has `clang-scan-deps-NN` and `/usr/lib/llvm-NN/lib/libc++.modules.json`. Pass that version to the toolchain via `set(VCPKG_ENV_PASSTHROUGH VVE_LLVM_VERSION)` in the triplet and `$ENV{VVE_LLVM_VERSION}` (default 18) in `linux-x64-llvm-libcxx.cmake`.
  - Wipe the build dir only when the cached `CMAKE_CXX_COMPILER` differs from the selected one.
  - Run `vcpkg install --triplet x64-linux-llvm` when vcpkg is found (`VCPKG_ROOT` or PATH).
  - Add `vcpkg-configuration.json` with `"overlay-triplets": ["./triplets"]`.
  - Drop `-DCMAKE_CXX_MODULE_STD` and `-DCMAKE_EXPERIMENTAL_CXX_IMPORT_STD`.
- **Done when:**
  - `grep -n "/home/hlavacs\|451f2fe2" build_linux.sh` is empty.
  - On a machine with only clang-20, running `VVE_LLVM_VERSION=20 ./build_linux.sh debug` twice gives a second run that prints no "recreating", with Ninja reporting "no work to do". The mtime of `build/debug-linux/_deps/*-src` is unchanged.
  - Plain `vcpkg install --triplet x64-linux-llvm` works from the repo root.
- **Risk / notes:** README:70 documents LLVM 18. Keep 18 as the default. The presets may stay on 18.

### B8 — Scope the import-std settings and the aligned-allocation workaround
- **Where:** `CMakeLists.txt:3-16` (`CACHE ... FORCE`), `:68-75` (`-fno-aligned-allocation` for `MATCHES "Clang"`); `build_linux.sh:81-82`; `cmake/toolchains/macos-arm64-homebrew-llvm.cmake:15`
- **Problem:** `CMAKE_EXPERIMENTAL_CXX_IMPORT_STD` and `CMAKE_CXX_MODULE_STD` are FORCE-written into the cache. That overrides user values and leaks into a parent project that adds VVE as a subdirectory, as ViennaPhysicsEngine does. `-fno-aligned-allocation` goes into the global `CMAKE_CXX_FLAGS` for every compiler whose ID matches "Clang": AppleClang, clang-18 (the Linux default), clang with the MSVC STL (debug-clang) and the fetched vvppl. The comment says it is only needed for clang 22 with libc++ `import std`. With the flag, any over-aligned `new` would silently get a misaligned block.
- **Change:**
  - Set both import-std variables as normal variables (no CACHE/FORCE) before `project()`. They are recomputed on every configure, so a CMake upgrade still picks the right UUID, and they do not reach a parent scope.
  - Add `-fno-aligned-allocation -Werror=over-aligned` to `CMAKE_CXX_FLAGS` only when `CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 22 AND NOT CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC"`. It must stay in `CMAKE_CXX_FLAGS` so the synthesized std module target gets it too.
  - Remove the duplicate settings from `build_linux.sh` and the macOS toolchain.
- **Done when:**
  - A clang-18 configure: `compile_commands.json` contains no `-fno-aligned-allocation`.
  - A clang ≥ 22 configure: every entry, including `__cmake_cxx_std_23`, contains `-fno-aligned-allocation -Werror=over-aligned`.
  - A wrapper project that uses `add_subdirectory(ViennaVulkanEngine)`: `cmake -LA -N` shows no `CMAKE_CXX_MODULE_STD` / `CMAKE_EXPERIMENTAL_CXX_IMPORT_STD` cache entries that VVE created.
- **Risk / notes:** `cmake/WindowsClangStd.cmake:3` sets `CMAKE_CXX_MODULE_STD OFF` after `project()`, in the same directory scope. It must stay after the new normal-variable set. A parent project must set the import-std gate itself before its own `project()`; document this in the README.

### B10 — Fix the Linux SDK rpath, the preset minimum version and the unused std.compat
- **Where:** `CMakeLists.txt:108-123` (SDK loader chosen as `Vulkan_LIBRARY`), `:400-405` (Linux rpath only `$ORIGIN/../lib`); `CMakePresets.json:3-7` (min 3.28); `CMakeLists.txt:1` (3.31.8); `cmake/WindowsClangStd.cmake:12`
- **Problem:**
  - With an autodetected or `VULKAN_SDK` SDK, executables that link `Vulkan::Vulkan` directly (all GPU tests) record `libvulkan.so.1`. With `BUILD_WITH_INSTALL_RPATH` their RUNPATH is only `$ORIGIN/../lib`, so at run time they load the system loader, or fail when none is installed. macOS appends the SDK lib dir (`:393-395`); Linux does not.
  - The presets claim CMake 3.28 while the project needs 3.31.8.
  - The Windows-Clang std target compiles `std.compat.ixx`, which nothing imports.
- **Change:** In the UNIX branch of `vve_set_output_dirs`, append the directory of `Vulkan_LIBRARY` to `INSTALL_RPATH` when it is not a system directory. That also covers build_linux.sh's `VulkanLoader/lib`. Set `cmakeMinimumRequired` to 3.31.8. Drop `std.compat.ixx` from `FILES`.
- **Done when:** With `VULKAN_SDK` set, `readelf -d <tree>/.../SimpleForwardRendererTests | grep RUNPATH` shows the SDK lib dir, and `ldd` resolves `libvulkan.so.1` there. `cmake --preset debug-linux` with CMake 3.30 fails in preset parsing. The debug-clang preset builds, and `ninja -t targets all | grep std.compat` is empty.

### B4 — Pin vvppl to an immutable, verifiable source
- **Where:** `CMakeLists.txt:173-178`; `README.md:5`; `build_windows_clang.cmd:17-24`
- **Problem:** `GIT_TAG v1.0` is a movable tag with no commit pin. The clone is a full, non-shallow git clone, and every clean configure of every tree needs the network. vvppl is the only dependency outside vcpkg. Today `v1.0` points to `e69f3e56beca935ceb257cce6005f6e39847bcb4` (checked with `git ls-remote`). A re-tag upstream would silently change the build, and could also trip the Apple FATAL_ERROR in B6.
- **Change:** Replace the git fetch with `URL https://github.com/orcunilker/ViennaVulkanPostProcessingLibrary/archive/e69f3e56beca935ceb257cce6005f6e39847bcb4.tar.gz`, `URL_HASH SHA256=<hash>` and `DOWNLOAD_EXTRACT_TIMESTAMP TRUE`. Compute the hash once with `cmake -E sha256sum` on the downloaded archive. This is preferred over `GIT_TAG <sha>` because it needs no git, downloads about 100 kB instead of the history, and the hash is verified. In the README, document `-DFETCHCONTENT_SOURCE_DIR_VIENNAVULKANPOSTPROCESSINGLIBRARY=<dir>` for offline builds. build_windows_clang.cmd already uses it. A vcpkg overlay port is not worth it: vvppl has no install rules and needs slangc at build time.
- **Done when:** `grep -n "GIT_TAG" CMakeLists.txt` is empty. A clean configure downloads the archive and verifies the hash. Editing one hex digit of the hash makes configure fail with a hash mismatch.

### B9 — Decide the AMD-layer workaround at run time, not by probing the build machine
- **Where:** `build_windows.cmd:80-103,112`; `src/CMakeLists.txt:16-20`; `src/versions/simple/Window.ixx:338-351`; `src/versions/simple/Render/RendererResources.cpp:40-44` (instance creation and device selection); `README.md:19,35`
- **Problem:** `build_windows.cmd` runs `vulkaninfo` on the build machine and bakes the result in as the `VVE_WINDOWS_DISABLE_AMD_SWITCHABLE_GRAPHICS` define. A binary built on a machine without the broken layer fails on one that has it. A binary built with the workaround disables a GPU-selection layer on machines that do not need that. A driver update requires a rebuild. The probe also costs two `vulkaninfo` runs on every build.
- **Change:**
  - Move the logic into `ForwardRenderer::init` (Windows only). If `instance.create()` or `physicalDevice.select()` fails, and `vkEnumerateInstanceLayerProperties` lists `VK_LAYER_AMD_switchable_graphics` that is not yet filtered: append it to `VK_LOADER_LAYERS_DISABLE` through a pure helper `detail::appendLayerFilter(std::string current, std::string_view layer)`, then `cleanup()`, log once, and retry once. The loader reads the filter variables on each instance creation.
  - Keep an explicit opt-in env var `VVE_DISABLE_AMD_SWITCHABLE_GRAPHICS=1` for the case where the layer crashes instead of returning an error.
  - Delete the CMake option, the define and the probe in the script.
- **Done when:**
  - `git grep -n "VVE_WINDOWS_DISABLE_AMD\|vulkaninfo"` finds only README text.
  - New non-GPU `tests/VulkanLayerFilterTests.cpp` asserts `appendLayerFilter("", L) == L`, `("A", L) == "A,"+L` and `("A,"+L, L)` unchanged.
  - Manual check: on the affected AMD machine, `game.exe` started from Explorer runs and logs the compatibility message once.
- **Risk / notes:** If the layer crashes the process rather than returning an error, only the env opt-in helps. Say so in the README.

### B11 — Embed the SPIR-V in the engine instead of loading it from an absolute build path
- **Where:** `src/versions/simple/CMakeLists.txt:36-47,72-111` (`VVE_SIMPLE_SHADER_DIR` = `<build>/src/versions/simple/shaders`); `src/versions/simple/Render/RendererResources.cpp:31-36,110-117`; `src/versions/simple/Vulkan/Pipeline.ixx:191-195` (`VulkanShaderModule::create` reads a file)
- **Problem:** The engine loads its three `.spv` files from an absolute path inside the build tree, compiled into the binary. Two things break. Executables copied to `<src>/bin` or elsewhere fail at renderer init (`platform_error`) once that build tree is deleted or moved. And with the shared `bin/`, which shaders a mirrored exe loads depends on which tree's .so it picked up.
- **Change:** Compile the shaders with slangc `-source-embed-style u32 -source-embed-name <name>` into generated headers, as vvppl does. List the headers as `OUTPUT`s and add them as sources of `ViennaVulkanEngine` so that editing the `.slang` rebuilds. Add `VulkanShaderModule::create(device, std::span<const std::uint32_t>)` and use it in `ForwardRenderer::init`. Remove the define and the file-reading overload if nothing else uses it.
- **Done when:** `git grep -n VVE_SIMPLE_SHADER_DIR` is empty. With the tree's `src/versions/simple/shaders` directory renamed, `SimpleForwardRendererTests` still passes. Touching `simple_forward.slang` rebuilds the engine.

## Phase 6 — Examples and low priority

### E1 — Clean up the examples and deduplicate their helpers
- **Where:**
  - `examples/testscene/testscene.cpp:268,302-305`; `examples/physics/physics.cpp:1-68`
  - `assetRoot`: `game.cpp:44`, `postprocessing.cpp:15`, `sponza.cpp:13`, `testscene.cpp:18`, `tests/RenderSponzaCaptureTests.cpp:29`
  - `frameLimit`: `game.cpp:70` (0), `light_shadow_debug.cpp:11` (1), `physics.cpp:11` (1), `postprocessing.cpp:41` (0), `simple_forward_demo.cpp:11` (3), `sponza.cpp:39` (0), `testscene.cpp:44` (0)
  - OBJ writer ×5: `Scene{AssetRemovalBlocked,Instantiation,PurgeUnusedAssets,Removal,System}Tests.cpp`
  - `imageStatistics` ×2: `RenderLightingCaptureTests.cpp:27`, `RenderSponzaCaptureTests.cpp:45`
  - `simple::Engine` hidden-window setup ×7 in 6 test files
- **Problem:**
  - `testscene` raises 8 of the 10 authored point intensities to 2.0.
  - `physics` has no physics: it is `simple_forward_demo`'s sample scene with a 1-frame default, so launching it from VS Code shows one frame and exits.
  - The copied helpers have drifted: `frameLimit` has four different defaults.
  - The `setCamera` extent part is handled by W3c, which removes the parameter.
- **Change:**
  - Delete `minimumShadowPointIntensity` and the clamp, and set the wanted intensities directly in the table.
  - Delete `physics` (preferred: `game` already shows per-frame object motion). Remove it from `examples/CMakeLists.txt`, `.vscode/launch.json`, README:66,105 and FACADE_AUDIT.md.
  - Add a module library `vve_example_support` (`examples/common/ExampleSupport.ixx`, module `VVE.ExampleSupport`) with `assetRoot(argv0)` and `frameLimit(argc, argv)`. `assetRoot` falls back to a `VVE_ASSET_ROOT="${PROJECT_SOURCE_DIR}"` compile definition. `frameLimit` uses one default: 0 = run until closed; CTest passes `--frames`.
  - Add a module library `vve_test_support` (`tests/support/TestSupport.ixx`) with `hiddenEngine(name, extent)`, `writeFixture(dir, name, text)`, `imageStatistics`, `meanAbsoluteDifference` and `pixelAt`. Use modules rather than headers so that they do not mix `#include <std>` with `import std`.
- **Done when:**
  - Each of `grep -rn "frameLimit(int argc\|assetRoot(char" examples tests` and `grep -rn "imageStatistics(const" tests` finds exactly one definition.
  - `examples/physics` is gone.
  - T10's example tests pass.
- **Risk / notes:** Do this after W3c and D11. `postprocessing` keeps using vvppl directly (DEC-8).

### R13 — Pin the spot-cone half-angle convention and verify non-glTF importers (low priority)
- **Where:** `S/Assets.ixx:354-355`, `S/Render/RenderSystemScene.cpp:262-264`, `S/Render/RendererShadowPrep.cpp:37,47-48`, `src/Types.ixx:75-77,95-96`
- **Problem:** The code consistently treats `SpotConeAngle` as a half-angle (shadow FOV = 2·outer; the shader compares the angle to the axis with `cos(outer)`), which matches glTF `KHR_lights_punctual` as copied by Assimp's glTF2 importer. Assimp does not normalise this across formats: the FBX and Collada importers pass the file's own angles, which I believe are full cone angles, and Collada even derives the outer cone from falloff and penumbra. Spots from those formats would then render up to twice as wide. The facade doc ("outer cone angle") names no convention.
- **Change:** Document the half-angle convention on `vve::SpotConeAngle`, `LightDescriptor::cone`/`inner_cone` and `SpotLight::cone`; this is a doc-only facade change. Export a known 60° spot from Blender as .gltf, .dae and .fbx and print `mAngleInnerCone`/`mAngleOuterCone`. Only if the non-glTF values turn out to be full angles, halve them in `AssetSystem::lightDescriptor` for those formats (by scene file extension). Leave glTF unchanged.
- **Done when:** `tests/AssetLightDataTests.cpp` adds a glTF spot (`innerConeAngle` 0.2, `outerConeAngle` 0.6) and asserts `cone.radians` ≈ 0.6 and `inner_cone.radians` ≈ 0.2 (±1e-5). If halving is added, a .dae fixture test asserts the halved outer angle.
- **Risk / notes:** Changing non-glTF angles changes how existing FBX/Collada scenes look.

## Image quality — VVE (IQ-00 to IQ-10)

The 2 November 2026 countryside brief governs these tasks; DEC-9 preserves the default path. VTL work is separate. Never replace preservation references with candidate output. Compare decoded RGBA with exact extent/alpha and fixture-specific limits frozen from pre-task repeat measurements; cross-driver/platform captures are separate evidence. Missing references fail. Changes require independent analytic/quality fixtures and deliberate failing controls, not just intermediate-pixel counts.

| ID | Work | Completion gate |
|---|---|---|
| IQ-00 | Audit the output chain and freeze immutable Linux preservation captures, provenance and repeat limits before renderer changes. | Three captures per fixture, archived outside disposable build outputs; existing failures recorded in the loop log. |
| IQ-01 | Verify existing msaa4 on forward, deferred and hybrid_gi, with/without native draws using context samples and attachment formats. | Analytic/supersampled coverage fixtures, capability rejection/recovery, preserved interiors and zero validation errors; document native pipeline requirements. |
| IQ-02 | Add the opt-in output chain and Khronos PBR Neutral tone mapping. | Independent fixed-vector/CPU reference, HDR unchanged, default preservation, encoding/GUI and fenced lifecycle checks; no new default-path resources or passes. |
| IQ-03 | Add opt-in SMAA 1x and msaa4_smaa inside VVE, with pinned reference sources and licence. | Frozen edge/detail limits, negative controls, cleared intermediates, supported formats, all backends/windows and synchronization-validated lifecycle tests. |
| IQ-10 | Finish VVE gallery, documentation and Linux verification evidence. | Inspected paired captures/manifests/differences, Debug/Release suites, actual format/backend coverage and physical-GPU timestamps; pending evidence remains incomplete. |

### IQ-03 blocker — AMD fixture repaired; frozen gates remain (iteration 25, 4 November 2026)

**Current status (iteration 26): the user-authorized acceptance reconciliation is implemented and verified below. The iteration-25 diagnosis is retained as historical evidence.**

Evidence is in `build/debug-linux/verify/smaa-quality/iteration-25/`: failing/working Slang and SPIR-V disassemblies, `spirv-val-*.{log,exit}`, `geometry-*.log`, original `quality-{debug,release}-amd.log`, `oracle-{debug,release}-amd.log`, `measured-amd.log`, `audit-amd.log`, exhaustive `failures-amd.tsv` / `preservation-amd.tsv` and inspected captures/differences. The physical device is AMD Ryzen 7 9700X / RADV RAPHAEL_MENDOCINO, Mesa 25.2.8-0ubuntu0.24.04.4, Vulkan 1.4.318, with `DISPLAY=:10.0`, `SDL_VIDEODRIVER=offscreen` and the Radeon ICD. Both checked-in Debug/Release builds (`--no-tests`, downloads disabled) and the configured job configure/build command pass.

The empty geometry is localized to RADV's handling of Slang's valid vertex-varying push-constant array load, not a layout/stride mismatch or VVE native-draw upload. Both SPIR-V variants pass `spirv-val --target-env vulkan1.2`; both declare `ArrayStride 16`, points offset 0 and offset/detail/padding at 48/56/60, matching the C++ 64-byte structure (checked by static assertions). The failing shader uses a dynamic `OpAccessChain` indexed by `VertexIndex`; the working shader uses constant indices 0/1/2 and equivalent vertex-ID selection. The fixture owns its pipeline layout and `vkCmdPushConstants`; VVE only supplies the context. The owning `tests/assets/smaa_fixture.slang` now uses constant selection, with identical vertices, push values, arithmetic and draw order. The same diagnostic fails before the fix with HDR red sum 0 and passes after it with 4441, matching the independent parallelogram determinant. Synchronization validation is explicitly active and reports zero errors in both variants. No VVE source fix was needed. Oracle/current-test banners now report actual device/driver receipts.

The original Debug and Release `RenderSmaaQualityTests` each still fail: all 60 forward `none` images and its frozen aggregate metrics/hash pass, then `msaa4` tilted/offset 0/repeat 0 fails exact decoded equality at `RenderSmaaQualityTests.cpp:365`. Its 149 changed RGB pixels have maximum error 1 byte, mean error 0.00909423828125 bytes/channel over the full image and unchanged alpha: 120 pixels are 188 versus frozen 187, and 29 are 224 versus frozen 225. This real offscreen surface is RGBA8 UNORM (37), SRGB_NONLINEAR; the freeze was llvmpipe/X11 BGRA8 sRGB. The historical UNORM quantization/CPU encoding path is unchanged. Thus the requested AMD none/msaa4 preservation pass cannot be claimed, and the original test does not reach its SMAA gates. No reference, limit or recording path was changed to bypass that early exit.

A separate build-tree C++ diagnostic continues all frozen inputs/evaluators without suppressing the original failure. On every backend, all 60 `none` comparisons pass; `msaa4` has 6 exact matches and 54 one-byte mismatches (all offsets/repeats listed in the TSV), so 180/180 none and 18/180 msaa4 match across three backends. Native repeats are byte-identical. The msaa4 ordered hash is `b8ca4f73d5718249` versus `frozenHashes[1]=c8e5b19846691cb0` (`SmaaQualityReference.hpp:24`); all five aggregate `frozenErrors[1]` exact checks also differ. Their measured/frozen pairs are tilted 0.011397149680729721/0.011331090528905335, thin 0.0072053510285706784/0.006978857010126477, corners 0.012437179841890611/0.012358454450816068, diagonals 0.012115902786609355/0.012029052826994664 and cutout 0.037543214790832363/0.037541147277326284 (`:20,22`, offsets 0..3 aggregated, every backend/repeat).

The original stage oracle passes all 144 cases in both Debug and Release on AMD, across forward/deferred/hybrid_gi and one/four scene samples: edges agree exactly, weights and final RGBA output stay within one byte. Independent CPU geometry and pinned `71c806a8` tables remain the sources. Patterned-to-flat edges/weights/output and bypass/blur/outside/regression controls pass in the continued diagnostic. Synchronization validation is explicitly active, with zero validation errors. The diagnostic's remaining engine/native quality failures are identical on all three backends (offset indices refer to the unchanged `offsets` array; constants/lines below are in `tests/support/SmaaQualityReference.hpp`):

| Fixture / mode | Metric | Measured | Frozen limit / constant |
|---|---|---|---|
| thin / smaa, offsets 0,1,2,3; msaa4_smaa, offset 0 | Relative coverage error | 0.04292599335012189 | 0.025 / `maxAreaRelative:14` |
| thin / smaa, offsets 0..3 aggregated | Aggregate edge-MAE ratio versus none | 1.0492211771947235 | 1.0 / `maxSmaaRatio[1]:10` |
| tilted / msaa4_smaa, offsets 0..3 aggregated | Aggregate edge-MAE regression versus msaa4 | 0.008390506601105348 | 0.00390625 / `maxCombinedRegression:13` |
| thin / msaa4_smaa | Same | 0.004448467564637894 | Same |
| corners / msaa4_smaa | Same | 0.007472628293707205 | Same |
| diagonals / msaa4_smaa | Same | 0.012373736515342106 | Same |

Independent ideal CPU reference SMAA also fails: thin coverage error 0.04304636580173435, thin aggregate ratio 1.0493749602875606 and combined diagonal regression 0.012570099679338502. The limits were derived from none/MSAA variation and analytic quadrature, not bounds on reference SMAA: its finite-bar endpoint/corner blending does not preserve integrated coverage, and filtering already-resolved edges adds smoothing. Maintainer reconciliation is required; limits, masks, controls and all 424 sealed freeze files remain unchanged. Separate same-device llvmpipe/X11 Debug image_quality verification is 17 passed / 1 failed / 0 skipped (`RenderSmaaQualityTests` only, 338.08 s), including default and legacy-UNORM preservation, tone mapping and output lifecycle tests; both additional RenderAntialiasingTests pass (2/2). This is separate preservation evidence, not substituted AMD coverage. Additional AMD offscreen related tests are 6 passed / 10 failed / 0 skipped in each build: six requested real-sRGB format cases are unavailable, two native-minimize cases are unsupported, and both existing RenderAntialiasingTests fail their capture path; the analytic MSAA quality, unsupported-capability, UNORM tone/matrix, lifecycle and CPU oracle tests pass. Their logs are retained without changing unrelated tests. AMD full-quality acceptance and 18/18 remain incomplete; no commit or merge.

#### Authorized acceptance reconciliation (iteration 26, 4 November 2026)

The maintainer explicitly authorized correcting this contract on 4 November; the iteration-25 diagnosis above remains historical evidence, not a requirement for reference SMAA to satisfy an impossible filter-quality bound. No renderer, reference shader/table, vvppl, historical image or archived manifest is changed. The original constants remain in `SmaaQualityReference.hpp`: thin relative area **0.025**, thin SMAA/none edge-MAE ratio **1.0**, combined-minus-MSAA edge-MAE **0.00390625**. Iteration-25 RADV observations were **0.04292599335012189**, **1.0492211771947235**, and combined tilted/thin/corners/diagonal regressions **0.008390506601105348 / 0.004448467564637894 / 0.007472628293707205 / 0.012373736515342106**. Independently computed ideal CPU HIGH itself gives thin area **0.04304636580173435**, thin ratio **1.0493749602875606**, and combined diagonal regression **0.012570099679338502**. Those deviations are intrinsic endpoint/corner smoothing and filtering of already resolved MSAA, not justification to raise global limits to candidate maxima.

`RenderSmaaQualityTests` now separates reference conformance, intrinsic quality and historical preservation:

- Historical none/MSAA decoded pixels, ordered hashes and aggregate metrics retain **exact equality** only when the original sealed receipt's device, driver name/info/version, Vulkan API, output format/colour space, SDL capture backend and mutable-view policy match. The fixed native/unlit/GUI-hidden/128²/three-ordinary-frame capture protocol is reported separately. Missing receipts or images still fail. RADV/offscreen RGBA8 UNORM versus llvmpipe/X11 BGRA8 sRGB is explicitly **inapplicable**, including when incidental pixels match; it is never called preserved. Every path additionally checks analytic top-left geometry, standard Vulkan sample positions and linear-UNORM-before-PNG versus sRGB attachment quantization, with one encoded-byte RGB allowance and exact alpha. CPU tests exercise provenance equality and deliberate field mismatches.
- Analytic inputs feed independent pinned `71c806a8` CPU HIGH with original AreaTex/SearchTex; GPU pixels never feed the reference. Edges must match exactly and RGBA8 weights within **one byte**. Final RADV RGB must match within **one encoded byte**, alpha exactly. On llvmpipe, the documented Mesa 8.8 model supplies weights and a per-pixel interval propagates one weight byte, half a 1/256 coordinate quantum and one linear interpolation byte through the normalized neighbourhood rule, including possible direction ties, followed by one output byte. Flat regions do not inherit a global worst-case colour tolerance. Final conformance is now asserted, not diagnostic.
- The 16² supersampled coverage image remains diagnostic truth. Tilted/corner/diagonal/cutout standalone improvement gates retain **0.99**; thin bars instead require CPU-reference conformance and the unchanged **0.85** contrast minimum. The combined cutout residual still requires **0.99**. Filtered area is bounded by the larger of the historical area limit and that fixture/phase's CPU-reference area error, plus only propagated per-pixel quantization uncertainty. Additional aggregate edge error beyond CPU HIGH is bounded by the same independently propagated uncertainty; no global combined-regression constant is raised. Geometry-only edge/support masks, outside-mask **one-byte** allowance, byte-identical same-device repeats and exact patterned-to-flat clearing remain enforced.
- Correct CPU reference, bypass/no-op, blur, corrupted edges/weights, outside-mask changes, stale data, missing alpha and deliberate additional combined regression all traverse the real acceptance predicates. The standalone CPU test exercises both ideal and Mesa sampler models; no replacement GPU baseline is generated.

Verification uses Clang/libc++ 18 and the existing Linux presets. On AMD Ryzen 7 9700X / RADV RAPHAEL_MENDOCINO, Mesa 25.2.8, Vulkan 1.4.318, `DISPLAY=:10.0`, `SDL_VIDEODRIVER=offscreen`, actual RGBA8 UNORM (37) / SRGB_NONLINEAR output, the focused quality/oracle/CPU CTests pass **3/3 Debug** (153.47 s) and **3/3 Release** (13.02 s), with zero failures/skips. Both stage-oracle runs pass **144/144**, with exact edges, weights/final output within one byte, all three backends and one/four scene samples. The historical comparison is explicitly inapplicable; analytic standard-sample coverage and transfer/quantization assertions pass. Khronos validation and synchronization validation are demonstrably enabled, with zero Vulkan validation errors. The expanded two-model CPU control matrix initially exceeded the old Debug-only 30 s timeout; its registration now allows 120 s and the final run takes 38.31 s. This changes time allowance only, with no skipped assertions; the earlier timeout and successful standalone rerun are retained in the evidence.

The complete `ctest --test-dir build/debug-linux -L image_quality -V` run is **18 passed / 0 failed / 0 skipped**, 419.06 s, on llvmpipe LLVM 20.1.2 / Mesa 25.2.8, X11 on private `:93` with Xvnc/xfwm4. Its actual BGRA8 sRGB (50) path matches the original joined IQ-00 receipt/freeze-log provenance and exercises **unchanged exact** none/MSAA pixel/hash/metric comparisons. All other default/legacy-UNORM, tone-mapping, AA, capability and lifecycle checks remain unchanged and pass. The log has 368 explicit synchronization-enable receipts and no Vulkan validation errors. This software X11 preservation/minimize/format coverage is separate from physical RADV offscreen evidence: no physical sRGB-swapchain or native-minimize coverage is claimed, and iteration-25 offscreen limitations were not disguised by altering unrelated tests.

Both preset configure/build commands, including the exact job command `cmake --preset debug-linux && cmake --build --preset build-debug-linux`, pass. Evidence is under `build/debug-linux/verify/smaa-quality/iteration-26/`: `final-{debug,release}-amd.{log,xml,exit}`, `debug-image-quality.{log,xml,exit}`, `debug-cpu.log`, job-command logs, source/integrity receipts and capture snapshots. The final lossless inspection index covers 5,913 snapshot PNGs (including retained diagnostic images), reduced to 581 unique 128² tiles on ten sheets; every tile was inspected, with byte-identical earlier sheets reused. Geometry interiors and cutout holes remain intact; reference endpoint/combined smoothing is visible, blur dims thin bars, the extra-regression control removes a bar, and cleared attachments/output are black. No candidate-derived references were recorded.

SHA-256 checks retain all **424 sealed SMAA files** in each Debug/Release tree, all 212 existing build-baseline files and all 120 external-archive files/manifests byte-identical. Only the three owning SMAA test files, their CMake registration and the two requested decision/brief documents change from task start. RendererSmaa*, vendored SMAA sources/tables, vvppl and VTL remain unchanged; pre-existing worktree changes are preserved. `git diff --check` and additional whitespace checks on the untracked owning test files pass. No commit, merge or push.

## Not done by decision

- **B2** (keep each build tree's outputs inside that tree): dropped by DEC-7; executables keep going to `<src>/bin`.

## Already fixed or obsolete

Checked against `8185b02`; nothing to do:

- **R11** (obsolete): not a hazard. vvppl v1.0 touches the host images only with `vkCmdBlitImage` (it reads src at `VVPPL.cpp:479` and writes dst at `:531`) and documents dst as last written by TRANSFER (`VVPPL.h:140-143`). Its compute dispatches use only its own ping-pong images, which it synchronises itself. So the TRANSFER source stages at `RendererDraw.cpp:273` (HDR begin) and `:383` (GUI barrier) are correct; re-check only if vvppl starts reading src or writing dst in a shader.
- **D2** (fixed): `RenderSystem::scenes_` is now `std::set<SceneHandle>` (RenderSystem.ixx:194). It stores handles only, `loadScene` no longer copies the Scene into it, and `removeScene` reads it (RenderSystemObjects.cpp:84). The leftover write-only `active_scene_` is handled in D11.
- **B3** (fixed): `cmake/CMakePresets.base.json:43-87` now uses the Ninja generator for debug-windows/release-windows (`strategy: external`, `cl`), the same generator `build_windows.cmd:107` uses for the shared `build/<variant>-windows` directory, so "generator does not match" can no longer happen.

## Origin

Review numbers (from the review of 27 September 2026) behind the task IDs: W1 = #34, W2 = #40, W3a–W3c = #41, W4 = #42, W5 = #43, W6 = #66, R1 = #55, R2 = #56, R3 = #57, R4 = #58, R5 = #59, R6 = #60, R7 = #62, R8 = #63, R9 = #64, D1 = #44, D3 = #46, D4 = #47, D5 = #48, D6 = #49, D7 = #50, D8 = #51, D9 = #52, D10 = #53, D11 = #54, D12 = #65, D13 = #67, D14 = #68, B1 = #69, B2 = #70, B4 = #72, B5 = #73, B6 = #74, B7 = #75, B8 = #76, B9 = #77, B10 = #78, T1 = #80, T2 = #81, T3 = #82, T4 = #83, T5 = #84, T6 = #85, T7 = #86, E1 = #87. All other IDs are new findings from checking `8185b02`.

## Loop log

One line per skipped, obsolete or rescoped task: `<ID>: <what happened>`.

IQ-00 (2026-11-03, VVE audit): inspected current guidance and sources at `59f56667951477407907aee2326245b54d14ad3f`. `Types.ixx` exposes `Antialiasing{none, msaa4}` (none default), all three RendererBackend values and `NativeDrawContext::samples` (Vulkan sample-count bits). `recordNative` supplies 1x for shadows/probe captures and the actual renderer count for main geometry. Facade `setAntialiasing`, `setPostProcessSetup`, `captureFrameLinear` and both `captureFrameToPng` overloads only forward; no SMAA or VVE tone-mapping API exists yet. No renderer, shader, resource implementation or public API changed in IQ-00.
IQ-00 (output chain): each WindowTarget owns single-sample RGBA16F hdrImage, optional msaaImage and optional per-window vvppl postProcess, with no postImage/ldrImage. sampleCount() is 4 only for msaa4. recordCommandBuffer resolves average coverage into hdrImage after scene composition, transitions HDR/swapchain to GENERAL, applies vvppl or NEAREST-blits HDR to swapchain, then optionally draws GUI through ownedGuiViews (UNORM) or the scene view, before readback/presentation. Acquire waits at TRANSFER. captureFrameLinear copies untouched resolved FP16 HDR without rendering another frame; captureFrameToPng renders a newly acquired frame and reads the swapchain before presentation. Its legacy R8/B8 UNORM branch encodes RGB with the sRGB curve on the CPU, preserving alpha/channel order through writeReadbackPng.
IQ-00 (capabilities): RendererResources checks the intersection of HDR/depth image sample counts and framebuffer colour/depth limits before 4x initialization; failure cleans up and returns feature-not-present, mapped to platform_error. Unknown AA returns invalid_argument before the initialized check; any valid late selection returns already_initialized. RendererDeferred additionally checks sampled attachment usages and sampleRateShading. G-buffer attributes/depth remain per sample; the lighting shader uses Texture2DMS/SV_SampleIndex, and only composed colour resolves. Existing RenderAntialiasingTests cover forward/deferred, two windows, resize, GUI and gain; hybrid/native analytic quality verification remains IQ-01.
IQ-00 (vvppl destination): read the pinned `e69f3e56beca935ceb257cce6005f6e39847bcb4` VVPPL.h apply contract and VVPPL.cpp implementation without modifying them. A non-swapchain destination is supported: any configured-size, single-sample, blit-capable colour image with TRANSFER_DST usage in GENERAL; the host prepares destination barriers. Compute uses private per-slot ping-pong images; the last operation on dst is vkCmdBlitImage (TRANSFER_WRITE). Source requires TRANSFER_SRC/GENERAL. A future HDR postImage is therefore compatible in principle, subject to actual format-feature/synchronization tests in IQ-02. The supplied tone-map compute effect is Narkowicz ACES; an unconfigured chain leaves the legacy direct blit.
IQ-00 (Linux environment): initial preset configure could not find SDL3 in this fresh worktree. Reconfigured with read-only CMAKE_PREFIX_PATH `/home/hlavacs/Dokumente/GitHub/ViennaVulkanEngine/vcpkg_installed/x64-linux-llvm` and CMAKE_MODULE_PATH at its `share/stb`; no installed dependency was edited. Debug uses Clang/libc++ 18.1.3, CMake 3.31.8, Ninja 1.11.1 and Vulkan SDK 1.4.350.1. Same-process native probe: DISPLAY=:10.0, WAYLAND_DISPLAY unset, XDG_SESSION_TYPE=x11, both SDL video override variables unset, SDL selected x11. AMD RADV reports no presentation support on this XRDP surface (no DRI3); llvmpipe LLVM 20.1.2 / Mesa 25.2.8 can present and was actually used. This is software-renderer evidence, not physical-GPU coverage or performance evidence.
IQ-00 (frozen references): Debug-only RenderImageQualityBaselineTests is registered via vve_add_engine_test, links VVE.TestSupport and is labelled gpu/image_quality. An explicit one-time recording produced 22 fixtures x 3 independent engines = 66 captures: lit/unlit/flat/plane lighting, lit/unlit Sponza-named Sea Keep, default/set backgrounds on forward/deferred, plus none/msaa4 triangles on forward/deferred/hybrid_gi. Warm-up is 8 frames (64 for hybrid), capture is the following frame, animation is fixed at zero, and GUI appears only as the AA corner marker. All 66 pairwise RGB comparisons had changed count/max/mean error 0; alpha and dimensions match exactly. The checked-in `tests/support/ImageQualityBaseline.hpp` freezes all three decoded FNV-1a64 receipts and exact per-fixture limits; future CTest runs require reference hashes/masks and reject missing/unreadable/truncated references. A one-byte defect is required to fail exact fixtures. Recording is forbidden once receipts are populated, and existing references are never overwritten.
IQ-00 (artifacts/inspection): `build/debug-linux/verify/image-quality/baseline/manifest.txt` records source/dirty-patch SHA-256, asset/dependency receipts, toolchain, device/driver, surface formats/colour space, camera, sequence and validation metadata. All captures and all repeat differences were opened in native contact sheets under baseline/inspection: striped lit/flat/unlit triangle, uniform purple plane, dark textured Sea Keep rock walls/tan floor, two uniform blues, and cyan tilted AA triangles/grey GUI marker; MSAA coverage edges are softer. Repeat differences are black. Full-frame masks preserve appearance only; independent material/analytic quality masks remain IQ-01..IQ-03 work. Surface format was B8G8R8A8_SRGB (50), SRGB_NONLINEAR (0), with mutable B8G8R8A8_UNORM GUI views. The production selector prefers sRGB and has no force option; the separate Profiles-layer matrix below subsequently exercises the offered UNORM fallback. Physical-GPU and Release preservation evidence remain separate work.
IQ-00 (archive): sealed the baseline directory read-only and archived it outside disposable build outputs at `/home/hlavacs/Dokumente/GitHub/ai-loop-baselines/J20261103-183417-403786/vve-debug-linux.tar.gz`, with an adjacent SHA-256 receipt; preserve both. Restore an absent baseline by extracting that archive under `build/debug-linux/verify/image-quality/`, never by rendering new references. The archive includes PNGs, masks, differences, contact sheets, source patch, input/dependency receipts, manifest and frozen header. Original raw metadata is retained; canonical manifest text removes only NUL padding from Vulkan fixed-size device/driver character arrays. The normal labelled CTest passed (1/1, no skips, 61.85 s), generating 66 fresh captures and 198 comparisons/difference images under `verify/image-quality/current` without altering any reference. Explicit --record was confirmed to reject the frozen receipts.
IQ-00 (pre-existing suite): before adding/rebuilding the new test, the original Debug suite executed all 226 tests under VK_LAYER_KHRONOS_validation with VK_LAYER_VALIDATE_SYNC=1 and passed 226/226 (0 failed, 0 skipped; 645.08 s). There are no pre-existing failing tests in this environment. Full output/build logs and the native presentation-probe source are retained beside the baseline archive in `logs/`. The first pristine configure lacked dependency discovery (resolved above); an initial 60-second crash-wrapper run was interrupted and was not counted as a suite result. The completed run used a 14400-second wrapper budget and the original 226-test registration.
IQ-00 (legacy UNORM follow-up): discovered the installed SDK Profiles layer's `exclude_formats` setting and forced the existing production selector to B8G8R8A8_UNORM (44), SRGB_NONLINEAR (0), with no mutable GUI views, by excluding only B8G8R8A8_SRGB format features. `VK_KHRONOS_PROFILES_EMULATE_PORTABILITY=false` is necessary: the first exploratory run with the layer's default portability emulation returned one synthetic portability-extension validation error; the corrected run and all references have zero errors. No renderer hook or behaviour change was needed. Four independent forward fixtures (background, lit material, lit Sea Keep, AA triangle with vvppl gain and GUI) each have three captures and their own zero-variation receipts/masks under `baseline-unorm/`; all 12 originals and differences were opened. The original sRGB references/archive remain unchanged. RenderImageQualityBaselineUnormTests uses the same native executable with --unorm, is registered when the SDK Profiles layer exists and is labelled image_quality. Final filtered CTest: 2/2 passed, 0 failed/skipped (74.12 s). The additional read-only archive is `/home/hlavacs/Dokumente/GitHub/ai-loop-baselines/J20261103-183417-403786/vve-debug-linux-unorm.tar.gz`, with adjacent SHA-256; restore it beside baseline, never regenerate it from candidate output.
IQ-00 (final packaging/verification): both format sets now live under the requested `build/debug-linux/verify/image-quality/baseline/`: the UNORM files were moved unchanged into `baseline/legacy-unorm/`. The canonical complete archive is `/home/hlavacs/Dokumente/GitHub/ai-loop-baselines/J20261103-183417-403786/vve-debug-linux-complete.tar.gz` with adjacent SHA-256; extract this complete archive under `build/debug-linux/verify/image-quality/` to restore both sets together. Earlier archives and recorded manifests retain their original checkpoint paths and bytes; no reference PNG/hash/limit was replaced. There are 26 fixtures, 78 original PNGs, 78 repeat differences and 26 full-frame masks, with zero measured RGB repeat variation throughout. Native tests and their record guards are Linux Debug only; other configurations/platforms need separate evidence. Final `cmake --preset debug-linux && cmake --build --preset build-debug-linux` succeeded. The full post-change run passed 227/227 (697.21 s); the separately added UNORM registration passed in the filtered 2/2 run. A sorted test-name audit confirms all 228 currently registered Debug tests have passed, with no failures/skips or missing registrations. All 226 formerly passing tests still pass. Logs and name inventories are beside the archives in `logs/`; rendering/public API/vvppl and VTL are untouched, and no commit or merge was made.
IQ-00 (packaging path check): moving the sealed UNORM set exposed a candidate-output path accidentally resolving inside the read-only reference directory; that exploratory CTest failed before any graphics work. Kept candidate output explicitly outside baseline, rebuilt successfully and reran the relocated UNORM CTest: 1/1 passed (10.27 s), zero validation errors. Reference bytes and comparison limits were unchanged; the complete archive's SHA-256 was verified. Guidance and unchanged source modification times were refreshed before finalizing.
IQ-01 (2026-11-03): added RenderAntialiasingQualityTests (gpu/image_quality), exact CPU polygon/pixel intersections checked against independent triangle-area vectors and a 4441-pixel determinant, four subpixel offsets, none/msaa4 on forward/deferred/hybrid_gi with engine meshes and native tilted quads; native pipelines use context.samples/colorAttachmentCount/colorFormats/depthFormat and reuse existing native Slang shader/build rules. Limits were fixed before measurement, without tuning: edge MAE <=0.14 and <=0.65 of none, interior RGB <=0.001, integrated coverage relative error <=0.005, minimum 150 edge/2000 interior samples. All six pairs measured 0.200842 -> 0.073709 edge MAE (ratio 0.367000), interior error 0.000195325 and maximum area error 0.000246134; bypassed-AA controls fail. The real Profiles-layer VP_VVE_single_sample test rejects 4x with platform_error, preserves startup AA/backend, verifies cleanup and recovers with none on all three backends. Renderer fix: none needed; no renderer, facade or default-output implementation changed. Khronos logs explicitly report Current Enables: VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT; zero validation errors, including teardown. Same-process DISPLAY=:10.0, WAYLAND_DISPLAY unset, XDG_SESSION_TYPE=x11, SDL overrides unset, SDL=x11 and Vulkan surface presentation=true; llvmpipe LLVM 20.1.2/Mesa 25.2.8 supplies software-renderer evidence only, physical GPU unverified. cmake --preset debug-linux && cmake --build --preset build-debug-linux passed; ./build_linux.sh debug passed 222/222 (419.96 s), then ctest --test-dir build/debug-linux -L racer --output-on-failure passed 8/8 (296.39 s): sorted inventories cover all 230 registrations, including all 228 IQ-00 tests, with no failures/skips. ctest --test-dir build/debug-linux -L image_quality --output-on-failure passed 4/4 (82.88 s), including both unchanged frozen baseline sets; final rebuilt quality/capability rerun passed 2/2 (13.81 s). Frozen header checksum unchanged. Opened all nine contact sheets under build/debug-linux/verify/image-quality/msaa/: cyan quads retain colour/area, 4x edges are smoother and 8x difference outlines weaker; masks show fixed red edges/green interiors, and recovery sheets show none. Final sheet hashes match inspected files. README, simple AGENTS and deferred-renderer docs state supported backends/native pipeline rules; evidence/logs/source receipts are beside the sheets. Preserved pre-existing IQ-00 changes; no commit, merge, dependency edit or VTL change.
IQ-02a (2026-11-03): added facade-only ToneMappingOperator/ToneMapping vocabulary and forwarding setters/getters, with transactional invalid-argument/capability checks. RendererToneMap.cpp owns on-demand per-window fullscreen Neutral pipelines/descriptors and RGBA16F postImage (TRANSFER_DST|SAMPLED) only for vvppl destinations; direct HDR sampling is added on demand through fenced HDR replacement, leaving fresh-default flags/resources and the historical blit path intact. Optional owners retire by submission serial, resize uses existing synchronization, and no device-idle wait was added. GENERAL vvppl destinations publish TRANSFER_WRITE to fragment sampling; shared intermediates use queue-ordered read/write barriers; acquire waits at COLOR_ATTACHMENT_OUTPUT for direct output and TRANSFER for legacy output. sRGB attachments encode linear output once; SRGB_NONLINEAR UNORM uses shader encoding, with per-recorded-frame encoding/alpha receipts preventing a second PNG conversion. Mutable GUI views retain authored colours; the opt-in sRGB fallback has an ImGui-compatible decode shader and tracked theme conversion. Khronos PBR Neutral is pinned to 180b1a7bddec33f73fe41712a2963cc3ad8e5547 with Apache-2.0 attribution; negative/NaN RGB becomes zero, +infinity becomes 65504, and exposed channels saturate at 1e6 to avoid overflow. Native RenderToneMappingTests/UnormTests/NoMutableTests cover forward/deferred/hybrid_gi, independent fixed vectors and CPU reference (276 fixture checks, maximum RGB error 1 byte), untouched FP16 HDR (relative 0.001 plus 1e-7), invalid setters/state/output, unsupported metadata rejection, fresh-default resource/pass invariants, fractional alpha, widgets/HUD and an identity-vvppl non-swapchain destination smoke test. Actual presentation: SDL x11, DISPLAY=:10.0, WAYLAND_DISPLAY unset, XDG_SESSION_TYPE=x11, SDL overrides unset; llvmpipe LLVM 20.1.2/Mesa 25.2.8, BGRA8 sRGB and Profiles-forced UNORM, plus Profiles-excluded mutable-format extension. Software-renderer evidence only. Khronos synchronization validation explicitly reports enabled; zero validation errors in the quality runs. cmake --preset debug-linux && cmake --build --preset build-debug-linux passed; ./build_linux.sh debug passed 225/225 (426.42 s, confirmed exit 0; an earlier post-completion wrapper receipt was 143), ctest -L racer passed 8/8 (300.61 s), and the final ctest -L image_quality passed 7/7 (91.17 s). Inventory audit covers all 233 registrations, zero failed/skipped. Both frozen baseline sets and unchanged MSAA quality/capability tests pass; preservation changed/max/mean RGB errors remain zero and frozen header/archive checksums are unchanged. Opened all 288 final tone/encoding/GUI captures and the baseline candidates/differences in native-generated sheets: expected flat swatches, compressed highlights, matching purple widget/HUD halves and fractional-alpha samples; preservation differences are black. Captures, inspection indices, input/PNG hashes, source patch, logs and test inventory are under build/debug-linux/verify/image-quality/tone-mapping/. Documentation and facade audit updated. IQ-02b's MSAA/resize/two-window/effect-composition/in-flight matrix remains pending, as do physical-GPU/Release evidence; SMAA, vvppl, installed dependencies and VTL are untouched. Preserved existing IQ-00/IQ-01 changes; no commit or merge.
IQ-02b (2026-11-03): completed tone-mapping matrix/default/lifecycle acceptance with native RenderToneMappingMatrixTests and actual sRGB, Profiles-forced UNORM and no-mutable-view registrations (gpu/image_quality). Forward/deferred/hybrid_gi each exercise none/msaa4, two windows, enabled 80x56 resize, identity/exact per-channel vvppl gain (2,.5,.25), and independently computed Narkowicz ACES exposure .85 then Neutral (deliberately mapping twice); CPU Neutral/fixed-vector helpers remain test-only. Ordinary submissions queue none -> Neutral 1 -> 2 -> none -> re-enable, invalid setters, independent recorded window selections, minimized snapshots, GUI and 24 additional toggles. Opt-in coherent output copies execute inside each ordinary submission, retain serial/extent/settings/pass/encoding/alpha receipts and reject reads before an observed slot fence; PNG conversion shares the production recorded-encoding path. Confirmed IQ-02a already retires complete owners by submission serial; exposed live/pending owner/descriptor counts (observed maxima 14/14, bounds 16/32, drained to 0/0), retaining the retired set receipt with its owning pool; no engine device/queue-idle wait added. Corrected the test's ImGui teardown boundary after an initial in-use-buffer validation failure. IQ-00 now renders untouched-default repeat 0 and explicit ToneMapping{} repeats 1/2, separately checks API equality and still compares every capture to the unchanged immutable hashes/limits (all changed/max/mean RGB errors zero); complete archive tar comparison and final archived header SHA-256 match passed. cmake --preset debug-linux && cmake --build --preset build-debug-linux passed; Debug ctest -L image_quality passed 10/10; ./build_linux.sh release passed 226/226 (374.53 s, script excludes racer); full Release CTest passed 234/234 (604.53 s). The first full Debug launch passed 197/236 with 39 GI failures because SDK layer discovery was absent; a direct validation-dependent smoke check and full rerun with VK_ADD_LAYER_PATH=/home/hlavacs/vulkansdk/1.4.350.1/x86_64/share/vulkan/explicit_layer.d, VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation and VK_LAYER_VALIDATE_SYNC=1 passed 236/236 (787.93 s), zero failed/skipped and no Vulkan validation errors; logs explicitly report SYNCHRONIZATION_VALIDATION_EXT. Same-process SDL=x11, DISPLAY=:10.0, WAYLAND_DISPLAY unset, XDG_SESSION_TYPE=x11, SDL overrides unset, presentation=true; llvmpipe LLVM 20.1.2/Mesa 25.2.8 supplies software-renderer evidence only, actual BGRA8 sRGB/UNORM plus excluded mutable extension, no physical-GPU/timing claim. Opened all 4,824 final matrix captures per configuration in native contact sheets, plus exactness/GUI and preservation candidates/differences: uniform blue/purple swatches follow exposure/composition, grey GUI corners remain intact and preservation differences are black; maximum scene/widget RGB error is 1 byte. Final decoded receipts match inspected images. Logs, hashes, source patch and inspection indices live under build/debug-linux/verify/image-quality/tone-mapping-b and Release captures under build/release-linux/verify/image-quality/tone-mapping-b. Updated README, simple AGENTS and architecture notes; SMAA, vvppl, installed dependencies, facade implementation and VTL untouched. Preserved earlier working-tree changes; no commit or merge.
IQ-03a (2026-11-03, freeze written before any SMAA engine code): repaired the interrupted test-only freeze; Antialiasing remains {none, msaa4}. Frozen fixtures tilted/thin/corners/diagonals/cutout use four offsets, independent 16x16 linear coverage and geometry-only masks with 24-pixel conservative pinned-filter support. Named limits: SMAA/none edge-MAE ratios [.99,1,.99,.99,.99] (thin is preservation-only), preselected combined residual=cutout with ratio .99 versus msaa4, maximum per-fixture aggregate regression versus msaa4=.00390625, relative coverage error<=.025 (minimum coverage .975), thin contrast>=.85, outsideByteLimit=1. Forward, deferred and hybrid_gi each measured three repeats of none=[.03106987701406931,.045449522630834516,.03155653651021725,.030798441497800116,.041116474117023266] and msaa4=[.011331090528905335,.006978857010126477,.012358454450816068,.012029052826994664,.037541147277326284] aggregate edge MAE; all 360 decoded RGBA captures had zero repeat variation, ordered none/msaa4 FNV-1a64 receipts 8fb02ade1fca61dd/c8e5b19846691cb0 on every backend/repeat. Worst measured coverage error=.011574074074074073 and thin contrast=1; the limits derive only from these none/msaa4 measurements and independent analytic references. Captured-none bypass fails the shared SMAA effect evaluator; captured-MSAA bypass fails the combined residual evaluator; CPU 3x3 linear blur fails the shared detail evaluator (contrast .665387298282272, coverage error .002001423639372588); deliberate 2/255 outside-mask change fails the shared outside evaluator (1/255 accepted), and excess-regression control fails. Default mode verifies CPU references/masks/controls and frozen none/msaa4 pixels/metrics, explicitly reports both SMAA modes pending, and never casts unsupported AA values. All 18 freeze contact sheets and six control sheets opened: softened MSAA geometry edges, intact thin features, visible residual cutout aliasing and blurred-control contrast loss. Evidence, sealed references/masks and logs are under build/debug-linux/verify/image-quality/smaa/; repeat --freeze is rejected. Debug Khronos synchronization validation is explicitly enabled with zero validation errors; llvmpipe LLVM 20.1.2/Mesa 25.2.8 is software-renderer evidence only. Vendored upstream hashes match, MIT notice added, build downloads no SMAA; IQ-00 references/limits, tone-mapping tests, engine code, vvppl, installed dependencies and VTL untouched; no commit or merge.
IQ-03a-impl (2026-11-04, iteration 8; BLOCKED by the frozen IQ-03a quality gates): independently audited the pinned SMAA 71c806a838bdd7d517df19192a20f0c61b3ca29d definitions and table-generation geometry. Fixed iteration 7's shared SearchTex deviation: SMAA.hlsl integration step 4 and its HLSL_4 SMAASampleLevelZero require linear clamp, whereas the GPU adapter used point sampling and the CPU oracle rounded lookup coordinates. Restored reference linear sampling in simple_smaa.slang and SmaaStageOracle.hpp. Confirmed original top-to-bottom AreaTex RG8 upload without a vertical flip; orthogonal five 16-wide slots in the left half, diagonal four 20-wide slots in the right half; pixel size (1/160,1/560), subtile height 1/7 (80 rows), zero subsample row; packed SearchTex scale (32,-32) and texel-centre bias (.5+33*direction,32.5); first diagonal RG/second diagonal GR ordering; positive reciprocal/extent RT metrics and the reference VS offsets on Vulkan top-left/Y-down pixel centres. Permanent CPU-only RenderSmaaCpuTests checks six hand-integrated L/U/Z areas (1/4, 1/132, 1/24), fixed texel addresses/byte flooring and a terminated 45-degree half-square (435/900 strict-side samples, complementary R=31/60 -> byte 131 at (102,42)); wrong row orientation/subsample controls fail. A hand-counted 64x64 square (area 4096, edge band 1532) and fixed sRGB grey .21586050011389926 independently verify measure's linear MAE/coverage/contrast, same-pixel thin ratio and combined regression, including a rejected excess-regression control. Frozen metric/reference code is unchanged; all AA modes use the identical geometry, four offsets and geometry/filter-support masks. Mesa 25.2.8's independently checked 8.8 UNORM coordinate and per-axis interpolation rounding explains the earlier GPU/ideal-oracle mismatch; the device-model oracle now matches all 144 forward/deferred/hybrid_gi SMAA/combined edge maps exactly and weight maps within the unchanged one-byte bound, while ideal linear CPU results remain separately reported. Restoring reference filtering changes thin coverage error .0431641881 -> .04715799993425031 (limit .025) and aggregate thin edge-MAE ratio 1.0495254854 -> 1.0537623039298696 (limit 1.0). All three backends and engine/native geometry still fail: combined regressions for tilted/thin/corners/diagonals are .008854292803992529/.005496006026521886/.007941908996813215/.012668094326407966 against .00390625 each (worst diagonal unchanged); cutout residual ratio .3397386844880498 passes .99. Independently checked ideal CPU filtering also conflicts: thin coverage .04304636580173435, thin ratio 1.0493749602875606 and combined diagonal regression .012570099679338502. These finite thin-end and already-resolved MSAA-edge fixtures conflict with the pinned HIGH spatial filter under the frozen choices; maintainer reconciliation is required, and no quality acceptance is claimed. Negative bypass/blur/excess-regression/outside-mask controls and patterned-then-flat zero edges/weights/output still pass. Filtered Debug image_quality is 12 passed/1 failed/0 skipped (259.12 s), solely RenderSmaaQualityTests. Full Debug is 238 passed/1 failed/0 skipped (895.91 s), again solely RenderSmaaQualityTests; 602 synchronization-enable receipts and zero Vulkan validation errors, with SYNCHRONIZATION_VALIDATION_EXT explicitly enabled. Both immutable IQ-00/default and legacy-UNORM baseline comparisons and fresh-default resource/pass invariants pass. Configure/build presets pass with downloads disabled. Inspected all 14 unique sheets representing 84 engine/native/repeat/oracle/control/stale-data sheets, plus the before-repair thin fixture: GPU/CPU pairs agree visually, thin endpoints dim, combined geometric smoothing persists, blur loses contrast and flat intermediates/output are black. All 424 sealed SMAA files, frozen headers, tone tests and vendored SHA-256 hashes remain unchanged. Evidence/logs/indices/hashes are under build/debug-linux/verify/image-quality/smaa/audit-8/ and captures under smaa/current/. llvmpipe LLVM 20.1.2/Mesa 25.2.8 is software-renderer evidence only; no physical-GPU or extended IQ-03b coverage claim. No limits relaxed, references re-recorded, dependency/vvppl/VTL edits, commit or merge.
IQ-03b (2026-11-04, iteration 10): added gpu/image_quality two-window output lifecycle coverage on forward/deferred/hybrid_gi with none/smaa/msaa4_smaa, one-window vvppl, fence-covered 80x56 resize, native/snapshot minimize-resume, GUI/HUD/alpha and five shutdown cycles/backend/format. Real BGRA8 sRGB/UNORM with mutable support enabled/disabled pass on llvmpipe/X11; 2,592 readbacks stay within 1 byte. Debug image_quality: 17 passed/1 known failed/0 skipped; full Debug: 243 passed/1 known failed/0 skipped (992.58 s), solely unchanged RenderSmaaQualityTests, zero validation errors. Shared helpers retain iteration-9 assertions; no new renderer idle waits; protected references/vendor hashes unchanged. Evidence: build/debug-linux/verify/image-quality/lifecycle-windows/. No commit or merge.
IQ-10 (2026-11-04, iteration 11): audited six renderer docs and SMAA notice/README; fixed Release validation opt-in (VK_LAYER_VALIDATE_SYNC=1), leaving assertions unchanged. Release compilation and Debug presets pass; script CTest 233/234, full Release 241/242, Debug image_quality 17/18, zero skipped, solely the unchanged RenderSmaaQualityTests blocker; sync validation active with zero errors. Protected hashes/idle waits unchanged; inspected SMAA/GUI sheets. Logs: build/release-linux/verify/image-quality/release/. llvmpipe/X11 software evidence; physical-GPU timings pending. No VTL work, commit or merge.
IQ-03 (2026-11-04, iteration 24): AMD RADV diagnosis confirms pinned-reference stage agreement (144 cases, edges exact, weights/output <=1 byte) using transient equivalent constant-index fixture input; the original AMD test encounters empty geometry from variable push-array indexing, while X11 lacks DRI3. Independent reference SMAA fails the frozen thin-coverage/edge-ratio and four combined-regression gates; see the IQ-03 blocker above and build/debug-linux/verify/smaa-quality/iteration-24/ for exhaustive metrics, logs and inspected captures. Debug/Release builds and the configured job command pass; separate unchanged llvmpipe preservation image_quality is 17 passed/1 known failed/0 skipped, additional AA tests 2/2, synchronization validation active with zero errors. Only this blocker note and loop-log line change; source/tests/limits/references unchanged, no commit or merge.
IQ-03 (2026-11-04, iteration 25): repaired the owning fixture shader with data-equivalent constant-index vertex selection; valid SPIR-V layouts and the unchanged test-owned push path localize the defect to RADV dynamic push-array loads, HDR red sum 0 -> 4441, validation/sync active with zero errors. Debug/Release builds and job command pass; both original AMD quality runs now pass none then fail the first msaa4 frozen pixel comparison (149 pixels, max 1 byte, UNORM versus the sRGB freeze); both AMD stage oracles pass 144 cases. Continued diagnostics enumerate every preservation/quality failure under build/debug-linux/verify/smaa-quality/iteration-25/; inspected geometry, differences, oracle/quality sheets and updated the existing blocker in place. Separate llvmpipe Debug image_quality is 17/18, extra AA 2/2; AMD related checks 6/16 per build expose offscreen limitations. Frozen inputs/limits, VVE source and dependencies unchanged; no commit or merge.
IQ-03 (2026-11-04, iteration 26): implemented the 4 November authorized acceptance reconciliation in the owning SMAA tests only: provenance-gated exact history, independent analytic coverage/transfer checks, asserted CPU HIGH edges/weights/output, reference-relative thin/combined limits and shared positive/fault controls. Original constants and all sealed references remain documented/unchanged. Final RADV Debug and Release focused runs each pass 3/3 with 144/144 stage oracles; the complete matching llvmpipe/X11 Debug image_quality run passes 18/18, zero skips, synchronization validation active and zero Vulkan errors. Expanded Debug CPU controls take 38–39 s; corrected only their old 30 s timeout to 120 s after preserving the failed attempt. Both preset builds and the job configure/build command pass. Inspected the final ten-sheet lossless union (581 unique tiles from 5,913 snapshot PNGs); primitive interiors/cutout holes stay intact, reference smoothing remains visible and blur/removed-bar controls are distinct. All 424 sealed files per build, baseline/archive hashes, RendererSmaa*/vendor/vvppl and VTL remain unchanged; only six authorized files differ from task start. Full receipts: build/debug-linux/verify/smaa-quality/iteration-26/. Physical sRGB/native-minimize and broader IQ-10 evidence are not claimed. No commit, merge or push.
IQ-10 verification (2026-11-04, iteration 27): Debug/Release scripts pass 237/237 and 235/235; llvmpipe/X11 :93 Debug image_quality 18/18 and full Release 243/243, zero skips/errors with active sync validation. RADV offscreen Release 226 passed/17 classified format/capture/minimize/cross-device-reference failures/0 skipped; job preset command passes. All 424 sealed files per build, 212 build baselines and archive hashes unchanged; code preserved. Logs/XML/exits/inspected captures: build/debug-linux/verify/final-iteration-27/. VTL API/integrity pass; its builds/suites remain pending under the time budget. No commit/merge/push.
IQ-10 final status (2026-11-05, iteration 33): native llvmpipe Debug IQ 18/18 and full Release 243/243; RADV 226/243 (7 proven pre-existing, 10 unsupported surface/minimize cases). Both 424-file seals/archive manifests, preset job and both diff checks pass; VTL IQ 33/33 per variant/full Release 195/195. Evidence: build/debug-linux/verify/final-summary/final-status.md; physical presentation/Windows/macOS pending and measured GPU budgets exceeded. No source edits, suite reruns, commit, merge or push.
