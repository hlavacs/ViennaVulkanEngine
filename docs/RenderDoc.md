# Inspecting frames with RenderDoc

Updated 29 October 2026 for V3 `24760ae1` (25 October). Screenshots/capture results described here
retain their original provenance; this update does not create a new capture.

[RenderDoc](https://renderdoc.org) captures one frame of a running program and
shows every pass, draw call and image of it. In a **Debug** build the simple
engine names its Vulkan objects and labels its passes through
`VK_EXT_debug_utils`, so the G-buffer images and shadow maps are easy to find.
Release builds do none of this and pay nothing for it.

## Capturing a frame

1. Build Debug: `build_windows.cmd debug --no-tests` to compile without running tests, or `./build_linux.sh debug` on Linux. In a configured tree, `cmake --build <build> --parallel` also builds without invoking CTest.
2. In RenderDoc choose *File → Launch Application*, set the executable, for
   example `bin\debug\exe\light_shadow_debug.exe` with the arguments
   `--renderer deferred`, and press *Launch*.
3. Press F12 (or Print Screen) in the program's window, or click *Capture Frame(s)
   Immediately* in RenderDoc. *Queue Capture* on frame N captures a fixed frame
   of a bounded run such as `--frames 300`.
4. Open the capture in RenderDoc's capture list.

Every example accepts `--renderer forward` or `--renderer deferred`; only the
deferred renderer has a G-buffer.

## What the capture shows

The *Event Browser* shows each frame as a tree of labelled regions:

```
Frame: <window title>
  Shadow maps
    Shadow: directional light 0 cascade 0 … cascade 3
    Shadow: spot light 0
    Shadow: point light 0 face +X … face -Z
  G-buffer: base + roughness, normal + metalness, emission + AO, depth
  Deferred lighting
  Sky and transparency
  Post-processing (or HDR to swapchain blit)
  GUI
```

The forward renderer has a single `Forward colour` pass in place of the
G-buffer, lighting and transparency passes.

The *Resource Inspector* and the *Texture Viewer* use these names:

| Name | Contents |
| --- | --- |
| `<window>: G-buffer 0 base colour RGB + roughness A` | linear base colour, roughness |
| `<window>: G-buffer 1 world normal RGB + metalness A` | world normal in [-1,1], metalness |
| `<window>: G-buffer 2 emission RGB + AO/flags A` | emission; alpha = AO + 2 × flags (unlit 1, water 8) |
| `<window>: depth D32` | scene depth, Vulkan [0,1] |
| `<window>: HDR colour` / `HDR colour 4x MSAA` | lit colour before post-processing |
| `Shadow map directional: N layers, 4 cascades per light` | one layer per light and cascade |
| `Shadow map spot: N layers, 1 per light` | one layer per spot light |
| `Shadow map point: N layers, 6 cube faces per light` | +X, -X, +Y, -Y, +Z, -Z per point light |

Each shadow layer view is named after its light, for example
`Shadow map directional: light 0 cascade 2` or `Shadow map point: light 1 face -Y`.
Pipelines (`G-buffer pipeline`, `Deferred lighting pipeline`, `Shadow depth
pipeline`, …), shader modules, samplers, swapchain images and command buffers
are named as well.

To look at the G-buffer, select an event inside `Deferred lighting`: the
*Pipeline State* view lists the G-buffer images and depth as the lighting
inputs, and double-clicking one opens it in the *Texture Viewer*. Normals are
signed, so set the range to [-1, 1]. Depth images, including the shadow maps,
are easiest to read with the range narrowed to about [0.9, 1]. Pick a layer of
a shadow array with the *Slice* selector. With 4x MSAA the G-buffer images are
multisampled; the viewer shows one sample at a time or their average.
*Save Texture* writes any of them as PNG, EXR or DDS.

## Notes

- Names and labels need `VK_EXT_debug_utils`. RenderDoc provides it while it
  captures, so they appear even without the Vulkan SDK's validation layer.
- RenderDoc reports a device as Vulkan 1.3 only when it supports
  `bufferDeviceAddressCaptureReplay`; otherwise it reports 1.2 and the engine,
  which needs Vulkan 1.3, refuses that device. Current NVIDIA, AMD and Intel
  drivers support the feature.
- `RenderDebugLabelsTests` checks the names and labels in Debug builds and that
  Release builds record none.
