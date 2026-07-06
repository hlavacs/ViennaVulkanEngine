# Release gate review

Reviewed after re-reading `AI_NOTES/discovery.md`.

## Scope

- `src/versions/simple/Vulkan/Device.ixx`
- `src/versions/simple/Render/RendererDraw.ixx`
- `src/versions/simple/Engine.ixx`

## Findings

### `src/versions/simple/Vulkan/Device.ixx`

- `validationLayerName` and `validationLayerAvailable()` are declared inside `#ifndef NDEBUG`.
- Their only references are inside the matching debug-only gate in `VulkanInstance::create()`.
- Release keeps `layers` and `validationEnabled` available and used by the ungated instance create path.
- Release-only compile hazards found: none found.

### `src/versions/simple/Render/RendererDraw.ixx`

- CPU shadow diagnostic temporaries and writes are contained inside two `#ifndef NDEBUG` regions.
- Ungated frame uniform creation still uses only symbols declared outside the gates.
- Ungated GPU/readback calls reference renderer members/helpers that are not declared by these gates.
- No variable declared before a gate becomes unused solely because the gated diagnostic blocks are removed.
- Release-only compile hazards found: none found.

### `src/versions/simple/Engine.ixx`

- `detail::debugDumpGraphHotkey` is declared only in debug builds and referenced only from the matching debug-only hotkey block.
- `writeDebugGraphs()` remains declared and defined for release; its release branch consumes `directory` with `(void)directory` and returns success.
- `graphFileStem()` is only needed by debug graph writing; leaving the private static member defined in release does not create a name or reference hazard.
- Existing graph members are still used by ungated `buildDefaultGraphs()`.
- Release-only compile hazards found: none found.

## Verification

Static review found no release-only compile hazards in the three existing `#ifndef NDEBUG` gates. No source changes were made by this review.

Per task constraints, no CMake/vcpkg build was run in the sandbox.

## Iteration 6

Per task constraints, the chained debug+release build verification was delegated to the task test command outside the sandbox, where the vcpkg lock is writable. No CMake/vcpkg build was run in this sandbox, and no source changes were made.

## Iteration 7

Iteration 6's noted test command was debug-only by mistake, so it did not verify the release preset or the `NDEBUG` compile path. This iteration's task test command performs the first real chained debug+release build with `debug-macos-arm64-llvm` and `release-macos-arm64-llvm` outside the sandbox.

## Iteration 8

Commands run from `/Users/hlavacs/GitHub/ai-runs/J20260706-132828-590060`:

```sh
cmake --preset 'release-macos-arm64-llvm' -DVCPKG_MANIFEST_INSTALL=OFF
```

Release configure outcome: failed with exit code 1 before compilation. No retry with `-DVCPKG_MANIFEST_MODE=OFF` was run because the failure was not a vcpkg manifest install or lock failure.

First blocking configure error verbatim:

```text
CMake Error at /Users/hlavacs/vcpkg/scripts/buildsystems/vcpkg.cmake:908 (_find_package):
  Could not find a configuration file for package "SDL3" that is compatible
  with requested version "".

  The following configuration files were considered but not accepted:

    /Users/hlavacs/VulkanSDK/1.4.350.0/macOS/cmake/SDL3Config.cmake, version: unknown
      The version found is not compatible with the version requested.

Call Stack (most recent call first):
  CMakeLists.txt:238 (find_package)
```

The release build command was not run because the release configure step did not generate a usable release build tree.

## Iteration 9

Confirmed the reused vcpkg installed tree exists at:

```text
/Users/hlavacs/GitHub/ai-runs/J20260706-132828-590060/build/debug-macos-arm64-llvm/vcpkg_installed
```

Confirmed SDL3 is present under:

```text
build/debug-macos-arm64-llvm/vcpkg_installed/arm64-osx/share/sdl3
```

Commands run from `/Users/hlavacs/GitHub/ai-runs/J20260706-132828-590060`:

```sh
cmake --preset 'release-macos-arm64-llvm' -DVCPKG_MANIFEST_INSTALL=OFF -DVCPKG_INSTALLED_DIR=/Users/hlavacs/GitHub/ai-runs/J20260706-132828-590060/build/debug-macos-arm64-llvm/vcpkg_installed
cmake --build --preset 'build-release-macos-arm64-llvm'
cmake --preset 'debug-macos-arm64-llvm' && cmake --build --preset 'build-debug-macos-arm64-llvm'
```

Release configure outcome: succeeded with exit code 0. CMake used the debug tree `vcpkg_installed` path and generated `build/release-macos-arm64-llvm`.

Release build outcome: succeeded with exit code 0. The release `NDEBUG` compile path built `src/versions/simple/Vulkan/Device.ixx`, `src/versions/simple/Render/RendererDraw.ixx`, and `src/versions/simple/Engine.ixx`.

Job test command outcome: succeeded with exit code 0.

## Iteration 11

Re-read `AI_NOTES/discovery.md` and `AI_NOTES/release_gate_review.md` before running verification.

Commands run from `/Users/hlavacs/GitHub/ai-runs/J20260706-132828-590060`:

```sh
cmake --preset 'release-macos-arm64-llvm' -DVCPKG_MANIFEST_INSTALL=OFF -DVCPKG_INSTALLED_DIR=/Users/hlavacs/GitHub/ai-runs/J20260706-132828-590060/build/debug-macos-arm64-llvm/vcpkg_installed
cmake --build --preset 'build-release-macos-arm64-llvm'
cmake --preset 'debug-macos-arm64-llvm' && cmake --build --preset 'build-debug-macos-arm64-llvm'
```

Release configure outcome: succeeded with exit code 0.

Release build outcome: succeeded with exit code 0. The release build log showed `examples/simple_forward_demo/simple_forward_demo.cpp` scanned, compiled, and linked to `bin/release/exe/simple_forward_demo`, confirming the new FPS-reporting code builds in the release `NDEBUG` path.

Job test command outcome: succeeded with exit code 0.

## Iteration 12

Re-read `AI_NOTES/discovery.md` and `AI_NOTES/release_gate_review.md` before running runtime verification.

Initial crash-safe wrapper attempt from `/Users/hlavacs/GitHub/ai-runs/J20260706-132828-590060` did not produce demo stdout/stderr:

```sh
/Users/hlavacs/GitHub/ai-loop/ai_run_crash_safe.bash -- bin/release/exe/simple_forward_demo
```

Exit code: 1.

```text
(lldb) target create "bin/release/exe/simple_forward_demo"
Current executable set to '/Users/hlavacs/GitHub/ai-runs/J20260706-132828-590060/bin/release/exe/simple_forward_demo' (arm64).
(lldb) run
error: process exited with status -1 (no such process)
```

Checked the demo source after the wrapper failure. The sample scene is created in code by `render_system.loadSampleScene()`, and the capture path is derived from the executable directory, so the required worktree-root launch directory is suitable. Retried once directly from the worktree root.

Command run from `/Users/hlavacs/GitHub/ai-runs/J20260706-132828-590060`:

```sh
bin/release/exe/simple_forward_demo
```

Exit code: 1.

Verbatim startup output:

```text
2026-07-06 16:45:23.175 simple_forward_demo[86092:10157111] Failure on line 688 in function id scheduleApplicationNotification(LSNotificationCode, NSWorkspaceNotificationCenter *): noErr == _LSModifyNotification(notificationID, 1, &code, 0, NULL, NULL, NULL)
2026-07-06 16:45:23.535 simple_forward_demo[86092:10157111] Error received in message reply handler: Connection invalid
2026-07-06 16:45:23.535 simple_forward_demo[86092:10157130] Connection Invalid error for service com.apple.hiservices-xpcservice.
simple_forward_demo engine=simple
simple_forward_demo failed: stage=engine init, error=platform_error
```

No FPS line was printed because the release demo failed during engine initialization before rendering frames.

Display and Vulkan environment diagnosis from the same shell:

```text
DISPLAY=
WAYLAND_DISPLAY=
XDG_SESSION_TYPE=
SSH_CONNECTION=
TERM_SESSION_ID=490960E4-12B2-4BB9-A9A3-179BF113C6BE
VK_ICD_FILENAMES=
VK_DRIVER_FILES=
DYLD_LIBRARY_PATH=
DYLD_FALLBACK_LIBRARY_PATH=
```

`vulkaninfo --summary` exit code: 1.

```text
[mvk-error] VK_ERROR_INCOMPATIBLE_DRIVER: Vulkan is not supported on this device. MoltenVK requires Metal, which is not available on this device.
ERROR: [MoltenVK] Code 0 : VK_ERROR_INCOMPATIBLE_DRIVER: Vulkan is not supported on this device. MoltenVK requires Metal, which is not available on this device.
WARNING: [Loader Message] Code 0 : terminator_CreateInstance: Received return code -9 from call to vkCreateInstance in ICD /usr/local/lib/libMoltenVK.dylib. Skipping this driver.
ERROR: [Loader Message] Code 0 : setup_loader_term_phys_devs:  Failed to detect any valid GPUs in the current config
ERROR at /Users/lunarg/Dev/macos-sdk-build/Vulkan-Tools/vulkaninfo/./vulkaninfo.h:249:vkEnumeratePhysicalDevices failed with ERROR_INITIALIZATION_FAILED
```

## Iteration 13

Commands run from `/Users/hlavacs/GitHub/ai-runs/J20260706-132828-590060`.

Verified the requested Vulkan SDK ICD directory before launching the demo:

```sh
ls -la /Users/hlavacs/VulkanSDK/1.4.350.0/macOS/share/vulkan/icd.d
```

Exit code: 0.

```text
total 16
drwxr-xr-x@ 4 hlavacs  staff  128 May 11 20:43 .
drwxr-xr-x@ 7 hlavacs  staff  224 May 11 20:43 ..
-rw-r--r--@ 1 hlavacs  staff  186 May 11 20:43 MoltenVK_icd.json
-rw-r--r--@ 1 hlavacs  staff  157 May 11 20:44 libkosmickrisp_icd.json
```

Retried the release demo once with the VulkanSDK MoltenVK ICD explicitly selected:

```sh
VK_ICD_FILENAMES=/Users/hlavacs/VulkanSDK/1.4.350.0/macOS/share/vulkan/icd.d/MoltenVK_icd.json bin/release/exe/simple_forward_demo
```

Exit code: 1.

```text
2026-07-06 16:50:33.084 simple_forward_demo[86557:10160000] Failure on line 688 in function id scheduleApplicationNotification(LSNotificationCode, NSWorkspaceNotificationCenter *): noErr == _LSModifyNotification(notificationID, 1, &code, 0, NULL, NULL, NULL)
2026-07-06 16:50:33.154 simple_forward_demo[86557:10160008] Connection Invalid error for service com.apple.hiservices-xpcservice.
2026-07-06 16:50:33.154 simple_forward_demo[86557:10160000] Error received in message reply handler: Connection invalid
simple_forward_demo failed: stage=engine init, error=simple_forward_demo engine=simple
platform_error
```

No FPS line was printed. The ICD retry still failed during engine initialization before any rendered frames were reported.

Checked whether Metal is visible from this worker shell:

```sh
system_profiler SPDisplaysDataType
```

Exit code: 0.

```text
Graphics/Displays:

    Apple A18 Pro:

      Chipset Model: Apple A18 Pro
      Type: GPU
      Bus: Built-In
      Total Number of Cores: 5
      Vendor: Apple (0x106b)
      Metal: Supported
```

## Iteration 14

Iteration 13 established that the worker sandbox has no WindowServer/Metal session access: the demo fails at engine initialization with `platform_error` even though `system_profiler` reports `Metal: Supported`. In-sandbox demo runs are therefore permanently ruled out.

Runtime FPS verification for this iteration is delegated to the task test command. The harness runs that command outside the sandbox, and it now chains the debug build, the release configure/build reusing the debug tree's `vcpkg_installed`, and a launch of `bin/release/exe/simple_forward_demo` from the worktree root. The demo's FPS output will appear in the test output for review next iteration.

## Iteration 15

Commands run from `/Users/hlavacs/GitHub/ai-runs/J20260706-132828-590060`.

Minimal release demo preflight:

```sh
ls -la bin/release/exe/simple_forward_demo
```

Exit code: 0.

```text
-rwxr-xr-x@ 1 hlavacs  staff  169136 Jul  6 16:41 bin/release/exe/simple_forward_demo
```

The release demo exists in `bin/release/exe` and is executable. This iteration's test command, run by the harness outside the sandbox, now actually chains the debug build, the release configure/build reusing the debug tree's `vcpkg_installed`, and a launch of `bin/release/exe/simple_forward_demo` from the worktree root.

## Iteration 16

This iteration's harness test command now verbatim chains the debug build, the release configure/build reusing the debug tree's `vcpkg_installed`, and a launch of `bin/release/exe/simple_forward_demo` from the worktree root. The expected success marker in the test output is a line starting with `Rendered ` containing `Render FPS:`.
