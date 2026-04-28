# Vienna Vulkan Engine

## Setup

This project uses `vcpkg` manifest dependencies for third-party libraries that are not already provided by the Vulkan SDK. `assimp` and `sdl3` are declared in [vcpkg.json](vcpkg.json) and installed into the repo-local `vcpkg_installed` directory. SDL3 is built with its Vulkan feature enabled so the examples can create Vulkan-capable windows.

The project expects Vulkan, Slang, GLM, and optional macOS Vulkan ICDs such as KosmicKrisp to come from the Vulkan SDK. On Windows, CMake resolves the SDK from `$ENV{VULKAN_SDK}`. On macOS, CMake also auto-detects SDK installs below `$HOME/VulkanSDK/*/macOS`.

All engine math should go through the exported `vve::math` abstraction layer instead of using raw `glm` types directly. The precision can be selected at compile time:

```powershell
cmake --preset debug-windows -DVVE_MATH_USE_DOUBLE=ON
cmake --build --preset build-debug-windows
```

With `VVE_MATH_USE_DOUBLE=OFF` the engine uses `float`; with `ON` it uses `double`.

### Vulkan ICD Selection

The engine links against the Vulkan loader, not directly against individual drivers. On macOS, KosmicKrisp is selected through its Vulkan ICD manifest (`libkosmickrisp_icd.json`) when that manifest is present in the Vulkan SDK or a system Vulkan install.

For VS Code and normal macOS GUI debugging, configure the default selector and then build:

```bash
cmake --preset debug-macos-arm64-llvm -DVVE_DEFAULT_VULKAN_ICD=kosmickrisp
cmake --build --preset build-debug-macos-arm64-llvm
```

The runtime resolves `libkosmickrisp_icd.json`, sets `VK_ICD_FILENAMES` before the engine creates its Vulkan instance, and prints the Vulkan devices and driver metadata exposed by the selected ICD. SDL window creation is left on the system/default display path so platform window discovery is not constrained by a specific Vulkan ICD. If `VK_ICD_FILENAMES` is already set, the engine respects it and does not override it.

For manual command-line launches, the same selector can be supplied per process:

```bash
VVE_VULKAN_ICD=kosmickrisp bin/debug/exe/game
```

A custom KosmicKrisp manifest can be supplied with:

```bash
VVE_KOSMICKRISP_ICD=/path/to/libkosmickrisp_icd.json VVE_VULKAN_ICD=kosmickrisp bin/debug/exe/game
```

The public `vve::Engine<>`, `vve::ECS<>`, and `vve::World` aliases are selected through a single namespace-style define:

```text
VVE_DEFAULT_ENGINE_NAMESPACE
```

This value is used directly in qualified names such as `vve::v3::...`, so no numeric selector layer is needed. `vve::ECS<>` and `vve::World` follow the engine namespace automatically.

The CMake target exposes matching cache variables. The current codebase supports value `v3`:

```powershell
cmake --preset debug-windows -DVVE_DEFAULT_ENGINE_NAMESPACE=v3
cmake --build --preset build-debug-windows
```

All example targets now follow that single engine namespace selection automatically.

The default setup is host-aware:
- Windows uses the `x64-windows` vcpkg triplet
- Linux uses the `x64-linux` vcpkg triplet
- macOS uses the `arm64-osx` vcpkg triplet

`vcpkg install` is an explicit bootstrap step. Configure and build consume the already-installed packages from `vcpkg_installed/<triplet>`.

Before the first build, run:

```powershell
vcpkg install
cmake --preset debug-windows   # Windows
cmake --build --preset build-debug-windows

# or

cmake --preset debug-linux     # Linux
cmake --build --preset build-debug-linux

# or

cmake --preset debug-macos     # macOS
cmake --build --preset build-debug-macos
```

Release builds use matching `release-*` presets, for example:

```bash
cmake --preset release-macos-arm64-llvm
cmake --build --preset build-release-macos-arm64-llvm
```

Executables and libraries are written below the selected build directory and mirrored to the project root `bin` directory. The mirrored path uses only the build variant, for example `bin/debug/exe/game` or `bin/release/exe/game`. Platform names such as `Mac`, `Windows`, or `Linux` are not used below `bin`.

VS Code is configured to use CMake Tools variants instead of presets so the `CMake: Select Variant` command offers `Debug` and `Release`. The VS Code variant builds use `build/vscode-debug` and `build/vscode-release`.

The VS Code Run and Debug list intentionally contains only five launch entries: `game`, `physics`, `sponza`, `world tests`, and `all tests`. Each launch asks for `Platform` (`Mac`, `Windows`, `Linux`) and `Variant` (`debug`, `release`) and then runs the matching build task before launch. Select the platform that matches the machine running VS Code; these launch options are shared across operating systems, not cross-compilers.

If CMake Tools asks for a kit on Apple Silicon macOS, select `Homebrew LLVM arm64`. The workspace also uses `cmake/toolchains/macos-arm64-homebrew-llvm.cmake` so stale AppleClang kit selections are redirected to the Homebrew LLVM compiler required for `import std`.

On Apple Silicon macOS with Homebrew LLVM, use the arm64 LLVM preset:

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

To build and run all unit tests from the project root:

```powershell
cmake -S . -B build/debug-windows
cmake --build build/debug-windows --config Debug
ctest --test-dir build/debug-windows -C Debug --output-on-failure
```

To list the registered tests without running them:

```powershell
ctest --test-dir build/debug-windows -C Debug -N
```

## Doxygen

If Doxygen is installed, CMake adds a `docs` target. Generate the documentation from the project root with:

```powershell
cmake -S . -B build/debug-windows
cmake --build build/debug-windows --config Debug --target docs
```

The generated output is written to [docs/build](docs/build). The HTML entry page is usually [docs/build/html/index.html](docs/build/html/index.html).
