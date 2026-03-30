# Vienna Vulkan Engine

## Setup

This project uses `vcpkg` manifest dependencies for third-party libraries that are not already provided by the Vulkan SDK. `assimp` is declared in [vcpkg.json](C:/data/GitHub/ViennaVulkanEngine/vcpkg.json) and installed into the repo-local `vcpkg_installed` directory.

`glm` is not installed through `vcpkg`. The project expects `glm` to come from the Vulkan SDK include directory.

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
