# Vienna Vulkan Engine

## Setup

This project uses `vcpkg` manifest dependencies. `glm` is declared in [vcpkg.json](C:/data/GitHub/ViennaVulkanEngine/vcpkg.json) and is installed into the repo-local `vcpkg_installed` directory.

Before the first build, run:

```powershell
vcpkg install
cmake --preset debug
cmake --build --preset debug
```

## VS Code

The supported VS Code bootstrap task is `Bootstrap Debug`. It runs:

```text
Vcpkg Install -> Configure Debug -> Build Debug
```

Use that task when setting up the project on a new machine.
