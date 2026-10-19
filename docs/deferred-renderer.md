# Runtime renderer selection

Both rendering paths are compiled into the simple engine. Forward rendering is
always the default; no CMake renderer option or process-wide selection is used.
The existing `VVE_ENGINE_IMPLEMENTATION_NAMESPACE` still selects the complete
engine implementation.

Select the backend through the facade in a user system's initialization hook,
or after `Engine::init()` and before the first `Engine::step()`:

```cpp
auto &render = world.get<vve::RenderSystem>();
if (auto result = render.setRendererBackend(vve::RendererBackend::deferred);
    !result) {
    return result;
}
// render.rendererBackend() == vve::RendererBackend::deferred
```

`RendererBackend::forward` restores the startup choice before rendering. All
windows belonging to that RenderSystem use the same backend; separate Engine
instances retain independent selections. Selection becomes immutable when GPU
rendering initializes on the first drawable frame. Later changes to another
backend return `Error::already_initialized`; selecting the active backend again
succeeds. An invalid enumeration returns `Error::invalid_argument`. Failed
selection calls preserve the previous choice.

After window initialization, selecting deferred probes the device selected for
the first rendered surface, and unavailable capabilities return
`Error::platform_error` with a Vulkan diagnostic. A selection made before
windows exist is a pending startup request: the same check occurs at GPU startup
and `step()` reports failure. Antialiasing selected later is also checked at GPU
startup. A failed startup releases partial GPU resources; the application may
select forward or reduce antialiasing and retry. No complete-forward fallback
occurs when deferred is selected.

All examples and the accessible physics game accept `--renderer forward` or
`--renderer deferred`. See [the complete application matrix](renderer-compatibility.md)
for bounded commands, scenarios, captures and verification status.

# Passes and shared state

The explicit dynamic-rendering sequence is:

1. Existing directional cascades, spot layers and point-light faces.
2. Opaque geometry and alpha-cutout materials into three G-buffer images and depth.
3. A fullscreen triangle computes lighting into the linear RGBA16F HDR image.
4. The existing forward shaders draw sky and sorted transparent objects, sprites
   and translucent animated water using the preserved opaque depth.
5. Existing VVPPL post-processing (or the existing HDR blit).
6. ImGui composition and presentation.

Scene/asset catalogues, material tables and texture colour-space handling, GPU
uploads, per-slot dynamic vertex buffers, visibility/frustum culling, shadow
packing and rendering, cameras, windows, resource retirement, readback and
post-processing remain shared. Material sampling and lighting each have one
Slang implementation used by both paths. Backend-specific code owns only the
G-buffer, sampled-image descriptors, geometry/lighting pipelines and their
recording. There is no render graph or second scene implementation.

# G-buffer

Each window owns these drawable-sized attachments with matching sample counts:

| Attachment | Format | Channels |
| --- | --- | --- |
| Base/surface | `R16G16B16A16_SFLOAT` | Linear HDR base RGB, roughness |
| Normal/surface | `R16G16B16A16_SFLOAT` | World-space signed normal XYZ, metalness |
| Emission/state | `R16G16B16A16_SFLOAT` | HDR emissive RGB, `AO + 2 * flags` |
| Geometry depth | `D32_SFLOAT` | Vulkan nonlinear depth in [0,1] |

In Debug builds these images carry their names in RenderDoc; see
[Inspecting frames with RenderDoc](RenderDoc.md).

The packed flags contain unlit (1) and analytic water (8). AO is [0,1]; gaps
between flag values keep decoding unambiguous. Half-float precision quantizes
material factors and AO slightly; it preserves HDR base/emission values and
avoids a position attachment. Geometry resolves material textures, alpha masks,
normal maps, inverse-transpose normal transforms and wave normals using the
same shader code as forward. Albedo/emission images retain sRGB decoding;
normal/roughness/metalness/AO maps remain linear.

Lighting reconstructs world position using the inverse camera projection-view
matrix and the current sample's pixel coordinate/depth. It uses Vulkan's [0,1]
depth convention and the existing projection's Y flip. Classic Blinn-Phong and
GGX/panorama modes, all light types, shadow PCF, unlit behaviour and height haze
use the common lighting function. Clear depth leaves the HDR background intact;
the subsequent sky pass fills uncovered samples.

The attributes consume 24 bytes per coverage sample, plus 4 bytes of depth.
Retaining float base/emission supports the existing engine's HDR material
factors without narrowing them to UNORM. Background samples carry no material.

# Antialiasing and capabilities

Both existing modes (`Antialiasing::none`, `Antialiasing::msaa4`) are preserved.
With MSAA4, the three G-buffer images, depth and intermediate HDR colour all have
four samples. Geometry evaluates material attributes at sample rate. The
lighting shader uses `Texture2DMS.Load(pixel, SV_SampleIndex)` and sample-rate
shading; it reconstructs and lights each surface independently. Sky and
transparent objects then compose into the same multisampled HDR target with
per-sample depth tests. Only the final lit/composited colour is averaged into
the single-sample HDR image consumed by VVPPL. Surface normals, material flags
and depths are never averaged together. GUI and shadow maps remain single-sample.
Vulkan's sample-shading coordinate rule is documented in
[FragCoord](https://docs.vulkan.org/refpages/latest/refpages/source/FragCoord.html).

Deferred requires at least three colour outputs, sampled RGBA16F and D32 images
at the requested sample count, and room for four additional sampled-image
bindings. MSAA4 additionally requires `sampleRateShading`. Logical-device creation
enables that optional feature whenever available, including forward startup.
The complete sampled/attachment image usage is queried, not just format feature
bits. Unsupported deferred capability does not disqualify a device from forward
rendering. The material texture descriptor ceiling reserves the extra four
bindings only for deferred.

G-buffer writes become shader-readable before lighting. Depth returns to its
attachment layout for forward composition; HDR writes are synchronized before
attachment loads and final transfer. Attachments shared by a window's two
frame slots have explicit dependencies across submissions on the shared graphics
queue. Resizing waits through the existing idle boundary, releases descriptors
before views/images and rebuilds all window-sized attachments. Minimized windows
retain their resources and resume through the existing swapchain recovery path.
Scene replacements continue using the existing submission-serial retirement
queue; no deferred resource owns scene data twice.

# Verification

Build using the existing workflow:

```sh
./build_linux.sh release
ctest --test-dir build/release-linux -L deferred --output-on-failure
ctest --test-dir build/release-linux -L example --output-on-failure
ctest --test-dir build/release-linux -L racer --output-on-failure
```

`RendererBackendTests` exercises the public API, independent instances, malformed
requests and late changes. When the SDK Khronos Profiles layer is installed,
`RendererBackendUnsupportedTests` advertises only two colour attachments through
the real Vulkan layer, checks deferred rejection with the active forward choice
unchanged, and renders/captures two forward frames. `DeferredRendererTests` and `DeferredRendererMsaaTests`
render identical deterministic scenes through both compiled paths, inspect
recorded pass tags/attachment metadata and retain forward/deferred PNGs plus
`comparison.txt` under `build/<variant>-linux/tests/tmp/DeferredRendererTests/`.
The comparisons use full-image and surface-only mean absolute differences with
a tolerance of 3 byte units, allowing half-float quantization and reconstruction
rounding. The MSAA normal-map scene permits 5 byte units on object pixels because
the preserved forward path shades material maps at pixel frequency, whereas the
deferred path interpolates and filters them at each depth sample. Its full-image
limit remains 3. Deferred variants of existing regression tests cover resize, multiple
windows/frames, minimization, materials, transparency, sprites, texture/geometry
replacement, sky, post-processing and GUI. Application captures and any remaining
blockers are recorded in the compatibility matrix.

For SDK-provided validation layers on Linux, expose the layer manifest and
library directories before a Debug test run:

```sh
export VK_LAYER_PATH="$VULKAN_SDK/share/vulkan/explicit_layer.d"
export LD_LIBRARY_PATH="$VULKAN_SDK/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
./build_linux.sh debug
```

Debug instances enable synchronization validation when the installed layer
supports its settings extension. Tests report validation errors, pass execution,
sample counts and capture paths. Physical-GPU and cross-platform verification
status must be read from the matrix; a software-Vulkan pass does not establish
Windows/macOS or physical-GPU coverage.

Final verification on Linux used Clang/libc++ 18, Vulkan SDK 1.4.350.1 and
llvmpipe (LLVM 20.1.2, Vulkan 1.4.318): all 157 registered Release checks passed
(149 through `build_linux.sh`, then the 8 Racer checks). All 28 deferred Debug
checks passed with the SDK validation layer and synchronization validation;
there were no validation or synchronization errors. The native Vulkan Profiles
capability-rejection check passed in both builds.

The paired native scene tests retain 72 PNGs across 18 states, two backends and
two sample counts. The maximum full-image difference was 0.18793 byte units.
The 4x normal-map object metric was 3.79187 (limit 5); every other object metric
was below 0.45431 (limit 3). Application verification additionally inspected and
compared all 22 forward/deferred capture pairs; see the matrix for each scenario.

Retained verification artifacts:

- `build/release-linux/verify/deferred-release-workflow.log`: exact build workflow and 149 results.
- `build/release-linux/verify/deferred-release-tests.log`: full native diagnostics for those checks.
- `build/debug-linux/verify/deferred-validation-tests.log`: 28 validation-enabled results and captures.
- `bin/release/verify/renderer_apps/`: full application logs, 22-pair comparison metrics/checker and supplementary PNGs.

Windows, macOS/KosmicKrisp and physical GPUs remain unverified. The accessible
AMD device node denied access in this session, so physical-GPU verification is
outstanding; every accessible application was exercised using software Vulkan.
