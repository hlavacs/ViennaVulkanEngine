# Hybrid Lighting Demo

Demonstrates dynamic light probe extraction from Gaussian Splatting environments to light traditional 3D meshes using Image-Based Lighting (IBL).

![Convolution Irradiance Cubemap Ambient Light from Gaussian Environment](demo_screenshot.jpg)

## Overview

This demo showcases a hybrid rendering approach that combines:
- **Gaussian Splatting environments** (volumetric, photorealistic scenes)
- **Traditional mesh rendering** (rasterized 3D objects)
- **Dynamic IBL extraction** (real-time light probe generation from gaussian environments)

Lighting information is extracted from gaussian splat environments to light traditional 3D objects placed within them.

## Features

- Real-time gaussian splatting rendering (adapted from [VKGS](https://github.com/jaesung-cs/vkgs))
- Dynamic cubemap generation from gaussian environments
- Two irradiance convolution methods:
  - **Box filter** (fast, hardware downsampling)
  - **Cosine-weighted hemisphere sampling** (slow, high quality, LearnOpenGL PBR technique)
- Configurable cubemap resolution (default: 1024x1024 per face)
- Configurable irradiance resolution (default: 64x64 per face)

## Prerequisites

- **Vulkan SDK 1.4+** (with `glslc` shader compiler in PATH)
- **CMake 3.20+**
- **C++20 compiler** (MSVC 2022, Clang, or GCC)
- **Gaussian splat assets** (`.ply` files)
- **3D mesh assets** (`.obj` files)

## Building

From the ViennaVulkanEngine root directory (requires `-DVVE_GAUSSIAN_ENABLED=ON`):

```bash
cmake -B build -DVVE_GAUSSIAN_ENABLED=ON
cmake --build build --config Release
```

## Running

```bash
build\examples\gaussian-splatting-cubemap-lighting-demo\Release\gaussian-splatting-cubemap-lighting-demo.exe
```

## Configuration

### Asset Paths

Edit `gaussian-splatting-cubemap-lighting-demo.cpp`:

```cpp
const char* gaussianPath = "assets/gaussian-splatting/InteriorDesign.ply";
const char* meshPath = "assets/standard/sphere.obj";
glm::vec3 meshPosition = glm::vec3(0.0f, 0.0f, 0.0f);
```

### Convolution Quality

**Default:** Box filter (fast, suitable for real-time)

To enable high-quality convolution, uncomment line 96 in `gaussian-splatting-cubemap-lighting-demo.cpp`:

```cpp
// m_gaussianRenderer->SetUseConvolutionFilter(true);  // Slow but accurate
```

**Performance impact:** Cosine-weighted convolution is ~4-10x slower than box filter.

### Cubemap Resolution

Default: 1024x1024 per face (environment), 64x64 per face (irradiance)

To change, modify `include/VERendererGaussian.h`:

```cpp
uint32_t m_cubemapResolution = 1024;     // Higher = sharper reflections, slower
uint32_t m_irradianceResolution = 64;    // Higher = smoother gradients, slower
```

### Ambient Fallback Color

When no gaussian environment is visible, a neutral grey is used. Configure via:

```cpp
m_gaussianRenderer->SetCubemapClearColor(glm::vec3(0.3f, 0.3f, 0.3f));
```

## Controls

| Input | Action |
|-------|--------|
| **W/A/S/D** | Move camera forward/left/backward/right |
| **Space** | Move camera up |
| **Shift** | Move camera down |
| **Mouse** | Look around (first-person camera) |
| **ESC** | Exit application |

## Scene Setup

The demo scene contains:
1. **Gaussian environment** - Loaded from `.ply` file, renders as volumetric splats
2. **PBR mesh object** - Placed in the environment, lit by extracted IBL
3. **Optional lights** - Default VVE scene lights (can be disabled)

The gaussian environment acts as both:
- Visual background (rendered directly)
- Light source (via cubemap → irradiance map → PBR shader)

## Technical Details

### Rendering Pipeline

1. **Gaussian Rendering** (Compute + Graphics)
   - Pipeline stages:
     1. Parse PLY
     2. Rank (frustum culling)
     3. Sort (radix, back-to-front)
     4. Project (screen space)
     5. Render splats (graphics pipeline)

2. **Cubemap IBL Generation** (On-demand, per frame)
   - Render gaussians to 6 cubemap faces (1024x1024 HDR)
   - Convolve to irradiance map (64x64, box filter or hemisphere sampling)
   - Transition to shader-readable layout

3. **Deferred PBR Rendering**
   - G-buffer pass: Normal, albedo, metallic/roughness, depth
   - Lighting pass: Direct lights + IBL ambient (samples irradiance cubemap)

### IBL Integration

The irradiance cubemap is bound to the PBR lighting shader at:
- **Set 2, Binding 3** (see `shaders/Deferred/PBR_lighting.slang`)
- **Sampling:** `Common_Light.slang`

## References

- **Vienna Vulkan Engine** - [GitHub](https://github.com/hlavacs/ViennaVulkanEngine)
- **VKGS** (Vulkan Gaussian Splatting) - [GitHub](https://github.com/jaesung-cs/vkgs)
- **3D Gaussian Splatting** - Kerbl et al., SIGGRAPH 2023
- **LearnOpenGL PBR IBL** - [Tutorial](https://learnopengl.com/PBR/IBL/Diffuse-irradiance)