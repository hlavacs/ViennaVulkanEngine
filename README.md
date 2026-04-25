# Vienna Vulkan Engine

## Setup

This project uses `vcpkg` manifest dependencies for third-party libraries that are not already provided by the Vulkan SDK. `assimp`, `glm`, `imgui`, and `sdl3-mixer` are declared in [vcpkg.json](C:/data/GitHub/ViennaVulkanEngine/vcpkg.json) and installed into the repo-local `vcpkg_installed` directory.

`SDL3` is installed transitively by `sdl3-mixer`. The project still expects Vulkan to come from a Vulkan SDK or system Vulkan installation. On Windows, CMake resolves Vulkan SDK CMake packages from `$ENV{VULKAN_SDK}/cmake`.

All engine math should go through the exported `vve::math` abstraction layer instead of using raw `glm` types directly. The precision can be selected at compile time:

```powershell
cmake --preset debug-windows -DVVE_MATH_USE_DOUBLE=ON
cmake --build --preset build-debug-windows
```

With `VVE_MATH_USE_DOUBLE=OFF` the engine uses `float`; with `ON` it uses `double`.

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

The supported VS Code bootstrap task is `Bootstrap Debug`. It selects the correct host preset automatically and runs:

```text
Vcpkg Install -> Configure Debug -> Build Debug
```

Use that task when setting up the project on a new machine.

To run unit tests in VS Code, use `Tasks: Run Task` and choose one of these tasks:

- `Run Unit Tests`
- `Build And Test Debug`

`Run Unit Tests` executes the existing Debug test build in the workspace:

```text
ctest --test-dir <active CMake build directory> -C Debug --output-on-failure
```

`Build And Test Debug` runs:

```text
Build Debug -> Run Unit Tests
```

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

The generated output is written to [docs/build](C:/data/GitHub/ViennaVulkanEngine/docs/build). The HTML entry page is usually [docs/build/html/index.html](C:/data/GitHub/ViennaVulkanEngine/docs/build/html/index.html).
