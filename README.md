# Vienna Vulkan Engine

## Setup

This project uses `vcpkg` manifest dependencies for third-party libraries that are not already provided by the Vulkan SDK. `assimp`, `imgui`, and `sdl3-mixer` are declared in [vcpkg.json](C:/data/GitHub/ViennaVulkanEngine/vcpkg.json) and installed into the repo-local `vcpkg_installed` directory.

`glm` and `SDL3` are not installed through `vcpkg`. The project expects both to come from the Vulkan SDK. On Windows, CMake resolves `SDL3` from `$ENV{VULKAN_SDK}/cmake`.

All engine math should go through the exported `vve::math` abstraction layer instead of using raw `glm` types directly. The precision can be selected at compile time:

```powershell
cmake --preset debug-windows -DVVE_MATH_USE_DOUBLE=ON
cmake --build --preset build-debug-windows
```

With `VVE_MATH_USE_DOUBLE=OFF` the engine uses `float`; with `ON` it uses `double`.

The public `vve::Engine<>` and `vve::ECS<>` aliases can also be selected at build time through simple namespace-style defines:

```text
VVE_DEFAULT_ENGINE_NAMESPACE
VVE_DEFAULT_ECS_NAMESPACE
```

These values are used directly in qualified names such as `vve::v3::...`, so no numeric selector layer is needed.

The CMake target exposes matching cache variables. The current codebase supports value `v3`:

```powershell
cmake --preset debug-windows -DVVE_DEFAULT_ENGINE_NAMESPACE=v3 -DVVE_DEFAULT_ECS_NAMESPACE=v3
cmake --build --preset build-debug-windows
```

All example targets also show the downstream-style usage directly in their CMake files, for example [examples/game/CMakeLists.txt](C:/data/GitHub/ViennaVulkanEngine/examples/game/CMakeLists.txt) and [examples/physics/CMakeLists.txt](C:/data/GitHub/ViennaVulkanEngine/examples/physics/CMakeLists.txt).

The default setup is host-aware:
- Windows uses the `x64-windows` vcpkg triplet
- Linux uses the `x64-linux` vcpkg triplet
- macOS uses the `x64-osx` vcpkg triplet

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
ctest --test-dir build -C Debug --output-on-failure
```

`Build And Test Debug` runs:

```text
Build Debug -> Run Unit Tests
```

## Unit Tests

To build and run all unit tests from the project root:

```powershell
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

To list the registered tests without running them:

```powershell
ctest --test-dir build -C Debug -N
```
