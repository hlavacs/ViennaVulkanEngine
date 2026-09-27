# Vienna Vulkan Engine

## Setup

This project uses `vcpkg` manifest dependencies for third-party libraries: Assimp, GLM, ImGui, SDL3, stb, and Vulkan Memory Allocator are declared in [vcpkg.json](vcpkg.json) and installed into the repo-local `vcpkg_installed` directory. SDL3 is built with its Vulkan feature enabled so the examples can create Vulkan-capable windows. The post-processing library vvppl ([ViennaVulkanPostProcessingLibrary](https://github.com/orcunilker/ViennaVulkanPostProcessingLibrary), tag `v1.0`) is not a vcpkg package: CMake downloads it with `FetchContent` during the first configure.

The project expects Vulkan, Slang (`slangc` compiles the simple engine's shaders during the build), and optional macOS Vulkan ICDs such as KosmicKrisp to come from the Vulkan SDK. CMake takes the SDK root from `VVE_VULKAN_SDK_ROOT` or `$ENV{VULKAN_SDK}`. If neither is set, it auto-detects SDK installs below `$HOME/VulkanSDK/*/macOS` on macOS and `$HOME/vulkansdk/*/x86_64` on Linux.

### Windows build and launch

From an ordinary PowerShell window in the repository, run:

```powershell
.\build_windows.cmd debug
```

Install Visual Studio with the C++ workload and C++ CMake tools, the Vulkan SDK, and vcpkg first. The script discovers the Visual Studio tools, finds vcpkg through `VCPKG_ROOT`, `PATH`, or `C:\vcpkg`, and runs `vcpkg install --triplet x64-windows` on every build so changed manifests are applied. It then configures, compiles, and runs the tests. Repeat the same command to rebuild and retest; no separate CTest path setup is needed. PowerShell requires the leading `.\` for scripts in the current directory.

The script also checks Vulkan device discovery with the SDK diagnostic tool. If discovery fails normally but succeeds with `VK_LAYER_AMD_switchable_graphics` disabled, it enables `VVE_WINDOWS_DISABLE_AMD_SWITCHABLE_GRAPHICS` for this build. The resulting engine disables that layer only inside its own process, including when `bin\debug\exe\game.exe` is launched from Explorer. Existing layer filters are preserved. This needs no persistent Windows environment setting or sign-out. Logs are saved as `build\debug-windows\vulkan-probe*.log` (or under `release-windows` for release builds).

Use `release` instead of `debug` for a release build (`release` is also the default without an argument), `--no-tests` to omit tests, or `--clean` to recreate that variant's build directory. The script uses the Ninja generator and the build directory `build\<variant>-windows`. Prerequisite, dependency, configure, compile, and test failures are identified separately.

For ICODA analysis, build a separate Clang version after installing the dependencies above:

```powershell
.\build_windows_clang.cmd
```

This requires Visual Studio's LLVM and CMake components. It configures the `debug-clang` Ninja preset,
builds the engine, examples and Microsoft's standard-library modules with Clang, and runs CTest.
The compilation database is `build/debug-clang/compile_commands.json`; reload VVE in ICODA after the build.
Use the libclang shipped with that same LLVM installation. Extra CMake options can be passed to the script,
for example `-DVVE_WINDOWS_DISABLE_AMD_SWITCHABLE_GRAPHICS=ON` when that workaround is needed.

All engine math should go through the exported `vve::math` abstraction layer instead of using raw `glm` types directly. The CMake option `VVE_MATH_USE_DOUBLE` (default `OFF`) selects the scalar type of that layer: `float` with `OFF`, `double` with `ON`. The simple engine currently requires `OFF`: its GPU mirror structs (`RenderVertex`, `FrameUniforms`, `GpuMaterial`) must match the `float` shader layouts, and `static_assert`s stop a build with `VVE_MATH_USE_DOUBLE=ON`.

### Vulkan ICD Selection

The engine links against the Vulkan loader, not directly against individual drivers, and the loader chooses the driver (ICD). On macOS, CMake writes ICD manifests for the drivers shipped with the Vulkan SDK into the build directory: `<build>/vulkan/icd.d/libkosmickrisp_icd.json` for KosmicKrisp and `<build>/vulkan/icd.d/MoltenVK_icd.json` for MoltenVK. To run on KosmicKrisp, point the loader at its manifest when launching:

```bash
VK_ICD_FILENAMES=build/debug-macos/vulkan/icd.d/libkosmickrisp_icd.json bin/debug/exe/testscene
```

The VS Code macOS launch entries and `tools/vscode/run-ctest.sh` set `VK_ICD_FILENAMES` this way, and CTest runs `PostProcessingSmokeTests` with `VK_DRIVER_FILES` pointing to the KosmicKrisp manifest. The engine itself does not choose an ICD at run time. The cache variable `VVE_DEFAULT_VULKAN_ICD` (`system`, `moltenvk`, or `kosmickrisp`; the macOS presets and `build_macos.sh` set `kosmickrisp`) and the manifest paths are passed to the engine library as compile definitions, but the simple engine does not read them. There are no `VVE_VULKAN_ICD` or `VVE_KOSMICKRISP_ICD` environment variables. On macOS, SDL is told to load the Vulkan loader library that CMake found (`SDL_HINT_VULKAN_LIBRARY`).

The public facade is the C++ module `VEEngine` in namespace `vve` (sources in `src/`). It is bound to exactly one engine implementation, selected by the CMake cache variable:

```text
VVE_ENGINE_IMPLEMENTATION_NAMESPACE
```

CMake compiles `src/implementations/<name>.ixx`, the only facade file that names the implementation, together with `src/versions/<name>/`. The facade keeps user code in namespace `vve`; implementation-specific code lives below the selected engine namespace (`vve::simple`).

The active educational implementation is `simple`, and it is the only one;
`v3`, `v4`, and `v5` are retired and are not built:

```powershell
cmake --preset debug-windows -DVVE_ENGINE_IMPLEMENTATION_NAMESPACE=simple
cmake --build --preset build-debug-windows
```

All example targets follow that single engine namespace selection automatically. The examples live in one folder each below `examples/`: `game`, `testscene`, `physics`, `sponza`, `light_shadow_debug`, `simple_forward_demo`, and `postprocessing`.

The presets and build scripts are host-aware:
- Windows uses the `x64-windows` vcpkg triplet
- Linux uses the repository's `x64-linux-llvm` overlay triplet so dependencies share the engine's Clang/libc++ ABI. The Linux presets and `build_linux.sh` compile with LLVM 18 and libc++ (`/usr/bin/clang++-18`, `/usr/bin/clang-scan-deps-18`, `/usr/lib/llvm-18/lib/libc++.modules.json`)
- macOS uses the `arm64-osx` vcpkg triplet

`build_windows.cmd` and `build_macos.sh` run `vcpkg install` automatically. `build_linux.sh` and the CMake presets do not, so on Linux, and when you use presets, installing the dependencies is an explicit bootstrap step. CMake consumes the installed packages from `vcpkg_installed/<triplet>`.

Before the first build, run:

```powershell
.\build_windows.cmd debug  # Windows: install dependencies, configure, build, test

# or

vcpkg install --triplet x64-linux-llvm --overlay-triplets=triplets
./build_linux.sh debug         # Linux: configure, build, test
# (or: cmake --preset debug-linux; cmake --build --preset build-debug-linux)

# or

./build_macos.sh debug         # macOS: install dependencies, configure, build, test
# (or: vcpkg install --triplet arm64-osx; cmake --preset debug-macos; cmake --build --preset build-debug-macos)
```

`build_linux.sh` and `build_macos.sh` accept `debug` or `release` (the default) and `--clean`. They use the build directories `build/<variant>-linux` and `build/macos-<variant>`. The presets use `build/<preset name>`.

Release builds use matching `release-*` presets, for example:

```bash
cmake --preset release-macos-arm64-llvm
cmake --build --preset build-release-macos-arm64-llvm
```

Executables are written to the project root `bin` directory, below a path that uses only the build variant, for example `bin/debug/exe/testscene` or `bin/release/exe/testscene`. Shared libraries (Linux, macOS) are built below the selected build directory and mirrored to `bin/<variant>/lib`. Platform names such as `Mac`, `Windows`, or `Linux` are not used below `bin`. The `light_shadow_debug` example writes its verification text and PNG to `bin/<variant>/verify`.

VS Code is configured to use CMake Tools variants instead of presets so the `CMake: Select Variant` command offers `Debug` and `Release`. The VS Code variant builds use `build/vscode-debug` and `build/vscode-release`.

The VS Code Run and Debug list contains `Windows Debug (choose executable)`, `game`, `testscene`, `postprocessing`, `physics`, `sponza`, `world tests`, and `all tests`. Except for the first one, each launch asks for `Platform` (`Mac`, `Windows`, `Linux`) and `Variant` (`debug`, `release`) and then runs the matching build task before launch. The Windows-only first entry builds the Windows debug variant and asks which executable to debug. Select the platform that matches the machine running VS Code; these launch options are shared across operating systems, not cross-compilers. On macOS the launch entries set `VK_ICD_FILENAMES` to the KosmicKrisp manifest in `build/vscode-<variant>`.

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

The build scripts run all tests after a successful build (`build_windows.cmd` skips them with `--no-tests`). To build and run all unit tests from the project root:

```powershell
cmake -S . -B build/debug-windows
cmake --build build/debug-windows --config Debug
ctest --test-dir build/debug-windows -C Debug --output-on-failure
```

To list the registered tests without running them:

```powershell
ctest --test-dir build/debug-windows -C Debug -N
```

For other build directories (`build/debug-linux`, `build/macos-debug`, ...) replace the `--test-dir` argument; add `-R <name>` to run selected tests.

Every test is a C++ executable built from one file in `tests/` and registered in [tests/CMakeLists.txt](tests/CMakeLists.txt) with `vve_add_engine_test`. CTest additionally runs `LightShadowDebugExample` (the `light_shadow_debug` example, which writes `bin/<variant>/verify/light_shadow_debug.txt` and `.png`) and `PostProcessingSmokeTests` (the `postprocessing` example for three frames). The rendering tests `SimpleForwardRendererTests`, `RenderTextureDedupTests`, `RenderMaterialImportTests`, `RenderLightingCaptureTests`, `RenderSponzaCaptureTests`, and `RenderMeshDedupTests` open hidden SDL windows (64 to 256 pixels) and need a Vulkan device that can present to them.

## Doxygen

If Doxygen is installed, CMake adds a `docs` target. Generate the documentation from the project root with the following commands (`build_docs.cmd` runs the same two commands):

```powershell
cmake -S . -B build/debug-windows
cmake --build build/debug-windows --config Debug --target docs
```

The generated output is written to [docs/build](docs/build). The HTML entry page is usually [docs/build/html/index.html](docs/build/html/index.html).
