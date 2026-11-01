# Runtime renderer selection

Contracts updated 31 October 2026 for the implemented approximate diffuse GI renderer.
GI-09 below records the current Linux regression and countryside evidence; dated phase
results retain their original provenance. The focused stability checks below also passed on
Windows with an NVIDIA GeForce RTX 3080 Laptop GPU (1x and 4x MSAA bounce/cadence, off-camera
transport and cloudy-sky capture slots). The full Windows/macOS application matrix remains unverified.
Terrain needs optional `tessellationShader`; a skipped terrain rendering assertion does
not establish graphics coverage. Run `ctest --test-dir <build> -N` for local registrations.

All renderer choices are compiled into the simple engine. Forward rendering is
always the default; no CMake renderer option or process-wide selection is used.
The existing `VVE_ENGINE_IMPLEMENTATION_NAMESPACE` still selects the complete
engine implementation.

Select the backend through the facade in a user system's initialization hook,
or after `Engine::init()` and before the first `Engine::step()`:

```cpp
auto &render = world.get<vve::RenderSystem>();
if (auto result = render.setRendererBackend(vve::RendererBackend::hybrid_gi);
    !result) {
    return result;
}
// render.rendererBackend() == vve::RendererBackend::hybrid_gi
```

`RendererBackend::forward` restores the startup choice before rendering. All
windows belonging to that RenderSystem use the same backend; separate Engine
instances retain independent selections. Selection becomes immutable when GPU
rendering initializes on the first drawable frame. Later changes to another
backend return `Error::already_initialized`; selecting the active backend again
succeeds. An invalid enumeration returns `Error::invalid_argument`. Failed
selection calls preserve the previous choice.

After window initialization, selecting deferred or hybrid GI probes the device selected for
the first rendered surface, and unavailable capabilities return
`Error::platform_error` with a Vulkan diagnostic. A selection made before
windows exist is a pending startup request: the same check occurs at GPU startup
and `step()` reports failure. Antialiasing selected later is also checked at GPU
startup. A failed startup releases partial GPU resources; the application may
select forward or reduce antialiasing and retry. No complete-forward fallback
occurs for either requested backend. See the capability requirements below.

All VVE examples accept `--renderer forward`, `--renderer deferred` or
`--renderer hybrid-gi`. The sibling physics game retains its existing forward/deferred options.
See [the complete application matrix](renderer-compatibility.md)
for bounded commands, scenarios, captures and verification status.

Deferred lighting and contact AO share world-position reconstruction with explicit FMA order.
The perspective divide can amplify small rounding differences in homogeneous coordinates into
millimetres of surface-position error, changing probe visibility in an otherwise static scene.
Source-level `precise` alone is insufficient with the Slang 2026.8 direct SPIR-V compiler;
the reconstruction uses explicit fused operations without changing the shader build flags.
Physical-sky probe captures also hash face-local pixels for cloud jitter, preserving the same
sky radiance when a probe moves between temporary capture-atlas slots.

`HybridGiBounceTests` checks static HDR across pipelined batches and consecutive captures,
including sunlit shadows, AO toggles, and a fixture translated by (91, 6, 130) metres.
`HybridGiSkyTests` checks identical SH coefficients when the same cloudy-sky probe moves between
slots in both volumes. These checks retain a one-binary16-step HDR tolerance and do not add
steady-state readbacks to the renderer.

## Native procedural geometry (GI-06a)

The facade supports one application-owned Vulkan recorder per RenderSystem,
shared by its windows, with no terrain-library dependency. VTL adopts this API
directly (GI-06b); its former staged forward-only patch is no longer applied.

```cpp
using NativeDrawCallback = std::function<std::expected<void, vve::Error>(
    const vve::NativeDrawContext &)>;
auto registered = render.setNativeDraw([owner](const vve::NativeDrawContext &c)
    -> std::expected<void, vve::Error> { return owner->record(c); });
render.notifyNativeDrawChanged(); // external vertices, materials, masks or transforms changed
auto cleared = render.setNativeDraw({});
```

Install, replace or clear before or after GPU startup. Backend selection retains
its startup-only error contract; all three backends accept the recorder. Without
one, the original draw path is retained.

| `NativeDrawPass` | Scope, after engine geometry | Attachments / samples |
| --- | --- | --- |
| `forward_opaque` | Forward backend solids, before engine sky/transparency | Linear HDR RGBA16F + D32, window 1x/4x |
| `gbuffer` | Deferred/hybrid opaque and alpha-cutout | Three RGBA16F + D32, window 1x/4x |
| `shadow` | Each active directional cascade, spot, point face; hybrid also has a cached probe-sun scope | Depth-only D32, 1x |
| `probe_capture` | Hybrid updated probe faces, after engine opaque/sky | Linear radiance/distance RGBA16F + D32, 1x |
| `forward_transparent` | Every backend, after engine sky/transparency, before post-processing/GUI | Linear HDR RGBA16F + preserved opaque D32, window 1x/4x |

Native transparent draws follow engine transparency; the caller sorts its own
surfaces. Sorting individual transparent objects across owners is not provided.
Transparent surfaces should depth-test, blend without depth writes and omit
G-buffer/shadow/capture draws. Native opaque surfaces use `forward_opaque` or
`gbuffer` according to the backend, without drawing twice in forward composition.

One call has `prepare=true`, outside rendering after this window/slot's fence
and before shadows/captures. Its pass identifies upcoming main geometry. Use it
for initial uploads and fenced buffer writes; barrier external transfer writes
to their consumers. It exposes borrowed shadow/probe views for descriptor preparation only;
prepare must not sample them before the engine's capture/shadow dependencies. Other calls have
`prepare=false`, inside VVE-owned dynamic rendering on the same command buffer.
Bind your own compatible pipeline, descriptors, vertices and push constants;
VVE restores the state needed by subsequent engine draws. Set viewport/scissor
to `viewportX/Y,width,height`, including nonzero capture-atlas offsets.

`NativeDrawContext` fields are borrowed, not owners:

| Fields | Contract |
| --- | --- |
| `device, physicalDevice, commandBuffer` | Dispatchable Vulkan handle bits in `uintptr_t`, matching the former patch convention; other native handle bits use `uint64_t`. No Vulkan headers or renderer classes enter the facade. |
| `window, frameIndex` | Independent window and fence-completed slot; application mutable storage must be keyed by both. |
| `colorFormats, colorAttachmentCount, depthFormat, samples` | VkFormat / VkSampleCountFlagBits values for compatible pipeline creation; no current pipeline/private descriptor ABI is promised. |
| `samplerAnisotropy, tessellationShader` | Enabled optional device features. |
| `view, projection, viewProjection, eye` | Actual view for this invocation, including light and capture views; column-major facade matrices with Vulkan projection Y flip. |
| `frustum` | Inward unnormalized world planes, left/right/bottom/top/near/far; `dot(plane,(world,1))>=0` is inside. Clip inequalities are `-w<=x,y<=w`, `0<=z<=w`, depth clear 1, engine compare LESS. |
| `faceIndex, cascadeIndex` | Cube face +X,-X,+Y,-Y,+Z,-Z; directional cascade or near/coarse probe volume. Unused indices are zero. |
| `lightType, lightIndex, probeShadow` | Packed light: 1 spot, 2 point, 3 directional; the bool distinguishes the cached volume-sun scope. |
| `captureFar` | Radial distance limit in metres for capture alpha. |
| `probeIrradianceViews, probeOrigin, probeControl, probeEye, probeCascades` | Shared sampling contract below; zero handles/count outside hybrid preparation and forward draw calls. |

Cull from the whole procedural scene against **this pass's** frustum, not the
main-camera-visible list. Keep per-view draw data immutable through submission:
append separate ranges, prepare a superset, or use push constants. Rewriting one
mapped range six times before submission incorrectly gives every face its last
contents, even in a fence-completed slot.

G-buffer encoding: target 0 = linear base RGB / perceptual roughness A; target
1 = signed unit world normal RGB / metalness A; target 2 = linear HDR emission
RGB / `(materialAO + 2*flags)` A. AO is [0,1], flags 0 for ordinary lit surfaces,
1 for unlit. VVE's deferred GI and contact AO then shade native surfaces. Do not
prelight these attributes. Preserve alpha-mask discards and material sidedness
in geometry, shadow and capture shaders.

Capture RGB is outgoing **linear untonemapped radiance**, using direct lighting,
shadows, emission and sky, without authored ambient, probe feedback, fog,
exposure, post-processing or GUI. Alpha is
`min(length(world-eye),captureFar)` in metres, negative on backfaces (including
two-sided surfaces), positive `captureFar` for sky. Integration reads alpha for
distance moments and inside-probe inactivation; D32 does not replace it.
Single-sided backs provide zero radiance but still signed visibility; two-sided
materials flip shading normals as needed. Forward HDR applies the application's
usual scene atmosphere before the engine's post-processing.

`AtmosphereSettings::distance_mist_start` and `distance_mist_end` optionally add a
smooth distance fade to `AtmosphereSettings::color`, independently of ordinary
height fog. Both zero disable it; otherwise `0 <= start < end` is required. Surfaces
inside `start` are unchanged, and surfaces at or beyond `end` are fully concealed.
The physical sky's low horizon approaches the same colour. Forward, deferred and
hybrid GI resolve share this effect; probe captures exclude it so camera mist never
becomes bounced illumination. Native forward draws must apply the same smoothstep
using the public atmosphere settings; native G-buffer draws receive it during resolve.

Hybrid preparation (binding only) and forward draw calls expose two borrowed RGBA32F `probeIrradianceViews` in
SHADER_READ_ONLY_OPTIMAL. Bind them as two SAMPLED_IMAGE descriptors, no sampler.
`probeOrigin` is near origin xyz / spacing w; `probeEye` is the main eye used
for coarse origin and visibility bias; `probeControl` is enabled / strength /
visibility enabled / AO enabled; `probeCascades` is zero without shared probes.
Use **`src/shaders/vve_irradiance.slang`** with that public directory on the
shader include path. Define `VVE_IRRADIANCE_ORIGIN`, `VVE_IRRADIANCE_EYE`,
`VVE_IRRADIANCE_VISIBILITY` and `VVE_IRRADIANCE_CASCADES` from those fields.
Default bindings are set 0, bindings 0/1; override `VVE_IRRADIANCE_SET`,
`VVE_IRRADIANCE_NEAR_BINDING` and `VVE_IRRADIANCE_COARSE_BINDING` together.
This is the same sampler/constants used by engine opaque and transparent shaders,
without a private uniform ABI or a duplicate GI implementation.

When enabled and strength >0, `sampleProbeIrradiance(world,unitNormal,coverage)`
returns coverage-premultiplied **E/pi**, including visibility, validity and
near/coarse fading. Replace diffuse fallback by
`fallback*(1-coverage*strength) + sampled*strength`; multiply diffuse albedo,
`(1-metalness)` and material AO once. Add direct, emission and specular separately.
Do not multiply by pi/exposure/albedo twice, or apply opaque contact AO to
transparency. At zero cascade count do not bind/sample absent images. Refresh
application descriptors after their slot fence if views change; current probe
images persist across resize.

GI-06b adds borrowed first-direct-sun array views, four world-to-shadow matrices,
main-view depth splits/view, actual reciprocal shadow dimensions, and a separate
cached capture-sun array view/matrix to `NativeDrawContext`. Preparation binds
these after its slot fence; sample them only in later forward/capture scopes,
after VVE's shadow write/read barriers. Shadow scopes must not sample them.
`sunShadowCount=0` / `captureSunShadowEnabled=false` disable their corresponding
lookups. They cover the first direct sun, not arbitrary custom-light shading.
This closes the native direct-radiance shadow gap without exposing private
uniform layouts or introducing a library-specific renderer dependency.

`RenderSystem::captureFrameLinear(window)` is an explicit diagnostic snapshot of
the last resolved RGBA16F HDR frame, converted to owning row-major `Vector<Vec4>`
before post-processing/exposure. It draws no extra frame and uses the existing
queue-ordered transfer/readback helper, without a device-wide idle wait.
`validationActive()` and `validationErrorCount()` expose the Debug renderer
messenger/callback count for facade-only integration tests. Release has no engine
messenger (`validationActive()==false`); its native GI CTests explicitly require
the external Khronos synchronization layer and reject its error diagnostics.
Ordinary frames do
not invoke these readbacks. Invalid/unrendered windows retain explicit errors.

### Countryside native adoption (GI-06b)

VTL selects this VVE source/build tree through its `VTL_VVE_ROOT` cache variable
(and Linux-script environment override). It requires the supported API and links
matching prebuilt modules; it applies no patch, stages no alternate renderer and
imports no private renderer modules. Both old backend choices remain available.
Its terrain, stones, trunks, masked leaves, shrubs, flowers and grass write this
G-buffer encoding and receive engine lighting. Independent light/probe frusta
select immutable complete local candidate ranges rather than main-visible lists.
Capture uses direct-only linear radiance, signed radial depth, existing masks,
mid-LOD crown meshes, at most 8 terrain tessellation, and a stable 128-candidate
per-tile grass cap. Missing far vegetation and unresolved leaf detail remain
capture-LOD approximations. No terrain/hydrology/growth contracts change.

The bridge removes its private depth images and prepare-time shadow rendering.
Forward water blends over preserved opaque depth, never blocks probes/shadows,
and shares the public irradiance sampler and ambient replacement convention.
Geometry/visibility/material/weather edits notify once; the animation clock does
not invalidate every frame. Camera motion uses existing engine volume scrolling.
The preview placement neighbourhood is still bounded independently of region size.
The five-view native HDR/PNG and vegetation participation checks are registered
as `TerrainHybridGi-*` in VTL; see its README for controls and transient receipts.

Current native transport, water, countryside and regression measurements are recorded
in [GI-09 Linux acceptance](#gi-09-linux-acceptance-31-october-2026). Earlier GI-06b logs
retain their own provenance under the job's transient report directory.

Replacement/clear retires the old callback's captured GPU owners through the
existing graphics submission/fence queue, **without a device-wide wait**.
Release outside strong ownership before engine destruction; weak references are
suitable for CPU controls. Teardown waits at its existing boundary and destroys
callback owners before the device. Failed startup releases the callback and
partial resources; reinstall for retry. Do not retain contexts, submit, wait,
begin/end VVE rendering or mutate/replace the RenderSystem inside the callback.
Exceptions or returned errors abort via acquired-image/semaphore cleanup and
report platform_error.

Installation, replacement, clear and `notifyNativeDrawChanged()` mark probe
radiance stale and invalidate cached procedural caster depth. Notify after
external vertex, mask, material, visibility, transform or capture-radiance edits;
coalesce edits before a frame. The next active capture budget redraws the cached
sun map once, then reuses it until another notification or sun/projection/engine
caster change. It does not upload engine meshes. Probe radiance converges through
the existing 64/128-frame medium/low sweep, retaining stale samples meanwhile.
Disabled captures defer sun-depth refresh. `clearScene()` leaves the recorder
installed; clear it explicitly to remove external geometry.

`HybridGiProceduralTests` registers seven serial `gpu;gi` checks: facade/pass
contracts at 1x/4x, off-camera procedural red/green bounce and removal, the
HybridGiLeak visibility-disabled reference (<0.30 retained), procedural main
and capture shadows (main-shadow fraction +0.15), native G-buffer receiving,
transparent indirect-only sampling/alpha linearity, and six no-recorder PNG
comparisons. Native shader recording uses only this public contract; existing
internal setup/HDR readback/cache hooks provide numerical diagnostics.
`VVE_NATIVE_TEST_REPORT_DIR` chooses transient artifacts. Run
`HybridGiProceduralTests save-baseline` **before** renderer edits for pre-change
PNGs. A fresh checkout without saved references compares clean control with
explicit clear, and reports that provenance, not historical equivalence.

GI-06a's pre-hook 1x/4x forward/deferred/hybrid PNG comparisons were byte-identical.
Current procedural transport, validation and disabled-equivalence results are
consolidated in GI-09 below; the original `gi06a-*` receipts remain in the job report directory.

## Approximate diffuse GI (GI-02)

Select `RendererBackend::hybrid_gi` (example CLI `--renderer hybrid-gi`) before
rendering begins. Forward remains the default. The new path reuses deferred
geometry, shadows, HDR composition and per-coverage-sample MSAA lighting.
Directional visibility, inside-probe inactivation, scrolling, bounded refresh,
spatial contact AO, material-aware capture/shadows and shared transparent lighting
are implemented. VTL adopts the native contract above. Sky, direct light and emission
retain their existing main-view shading. This is approximate diffuse GI with one bounce.

```cpp
vve::GiSettings gi;                      // enabled=true, AO=false, strength=1, medium
gi.quality = vve::GiQuality::low;        // one probe/frame; medium updates two
gi.indirect_strength = 0.8F;            // finite and >= 0
if (auto result = render.setGiSettings(gi); !result) return result;
auto current = render.giSettings();
```

`setGiSettings` is a transactional runtime control; negative/nonfinite strength
and unknown presets return `invalid_argument` without changing any field.
`ao_enabled` independently enables contact AO on opaque indirect diffuse light. Controls are
per engine and shared by its windows; forward/deferred ignore them. Disabled
GI or strength zero with AO off bypasses probe sampling and matches deferred, including its
constant ambient/environment terms. Presets retain the same coverage and allocation
sizes but change the capture budget. GI-07 replaces their probe resources transactionally
through submission fences; see the lifecycle contract below.

`shaders/simple_shared.h` defines the grid dimensions, face size, spacing,
maximum budget and capture clipping distances once for C++ and Slang.
`IrradianceProbes` exposes these constants and checks the shared dimensions
with `static_assert`; the ordinary shader build tracks changes to that header.

Each window owns **two independent 4x4x4 toroidal grids (128 probes total)**:
near spacing **2 m**, coarse spacing **8 m**. The same capture/integration,
scrolling, inactivation and visibility code takes a cascade parameter; parameters
come from `simple_shared.h`. For each spacing `s`, the origin is
`(floor(camera / s) - 2) * s`, componentwise. Near/coarse spans are **6/24 m**;
the outer **one cell (2/8 m)** inside each edge is the blend band. Sampling uses
near light first, fills its uncovered share from coarse, then fills the remaining
share from the existing ambient/environment response. With premultiplied SH
samples `N,C` and valid/faded coverages `n,c`, the result is
`N + (1-n)*C + (1-n)*(1-c)*fallback`. Invalid, inactive and never-captured coarse
corners supply fallback, never black. `sampleProbeIrradiance` exists once in
the public `src/shaders/vve_irradiance.slang`; the engine's
`simple_irradiance.slang` supplies its bindings for deferred and forward transparency.

Storage is `row = wrap(x) + side*(wrap(y) + side*wrap(z))` for absolute world
cells; the shared positive-modulo macros handle negative coordinates. Each
cascade snaps and scrolls independently, retaining overlapping SH, moments and
ages exactly. Exposed rows are cleared, including validity and moments, before
sampling. One axis-cell shift retains 48 rows and exposes 16. A move of **one
coarse cell (8 m)** resets near but retains coarse overlap. A snapped near-origin
jump **greater than 8 m on any axis**, or scene replacement, resets **both**
volumes: the first successfully rendered frame has zero captures and uses exact
fallback. Initial allocation can capture immediately. Skipped/minimized frames
do not advance convergence. Uncaptured/inactive rows never count as black,
valid probes; CPU captured counters include inactive rows, so tests also read
actual GPU validity.

After this frame's shadow maps, capture **one (low) or two (medium) complete
16x16-per-face cubemaps**. Each cascade's 96x32 RGBA16F atlas and D32 depth are reused; the capture budget is shared across both volumes;
transparent/water/sprite items are excluded. Every face culls the complete
resolved instance list against its capture view, independently of main-view
visibility. Capture uses the shared material/lighting includes, texture alpha
cutouts, material-sided radiance, direct lights/shadows, emission and sky. Camera haze,
post-processing, GUI, exposure and all diffuse/specular environment ambient
are excluded from surface captures. There is no sampling of earlier probe
lighting, accumulation or multi-bounce feedback.

Sky items are drawn after opaque geometry into every capture face, using the
main view's shared sky vertex/fragment paths. Empty directions therefore store
the panorama's level-zero linear radiance or the physical sky's `skyColour`
(single scattering, sun and clouds). Per-face uniforms disable camera weather
haze; surface captures also bypass `applySkyAtmosphere`. There is no exposure,
tonemapping or GUI in these captures. The background colour is the fallback
only when neither a panorama nor the physical sky is configured. Capture alpha
stores **linear radial distance in metres**, clamped to the **64 m near / 256 m coarse** capture range,
negative for `SV_IsFrontFace == false`. Empty/sky directions store positive
64/256 m respectively. Capture near planes are **0.05/0.20 m**; radial means,
second moments (m²), visibility biases and the variance floor scale consistently
with cascade spacing. This data survives the D32 attachment's `DONT_CARE` store operation;
material alpha cutouts discard before writing radiance or distance.

The first packed lit directional light (the sun) has a **separate 512x512 D32
probe-shadow map per window**, independent of camera FOV, clipping planes and
shadow distance. An orthographic projection encloses the union of both volumes,
padded by **one coarse cell (8 m)**, using a quantized enclosing-sphere radius
(35 m for the current layout). Its **70 m footprint** is snapped to shadow
texels (70/512 m); its depth extends upstream by the coarse capture range to
include remote sunward casters. Capture lighting uses this transform and map,
not a main-camera cascade. Other directional lights retain their existing
cascade coverage. Direct-sun shadow coverage is bounded to this padded local
footprint; it is independent of main-view visibility.

The map reuses the existing shadow pipeline, material cutout test and sidedness
policy. The complete instance list is culled against **its own light frustum**,
never the main-camera visible list. Depth is cached until its fitted union/sun
direction or shadow-caster topology, masks, vertices, visibility or transforms
change. Intensity, background/sky/weather, pure color/roughness and main-view
rotation/shadow reach do not redraw unchanged depth. Pending projection changes
survive skipped acquisition until recorded. Shared frame-slot fences protect
capture uniforms/descriptors; explicit fragment-read -> depth-write and
depth-write -> fragment-read barriers serialize cached-map reuse and
probe-shadow publication before capture. No new idle wait or ordinary readback
is introduced. Main-view shadows continue to use their unchanged cascades.


A fullscreen GPU integration pass writes four RGBA32F SH texels per probe, with
explicit validity in alpha, followed by a flattened **8x8 octahedral map** in
the same image row (68 columns total). Its RG channels store the directional
mean radial distance (m) and mean squared distance (m²), integrated with the
cubemap solid angle and a `max(dot(direction, ray),0)^64` kernel. ZW retain
the capture's backface/very-short-hit solid-angle fractions for diagnostics.
Map dimensions and atlas stride come from `simple_shared.h`, with C++
`IrradianceProbes` static assertions. The orthonormal real **L1 RGB SH** basis is
`Y00=0.2820947918`, `Y1xyz=0.4886025119 * direction.xyz`. Cubemap midpoint
quadrature uses the solid-angle Jacobian `(1+u²+v²)^(-3/2)`, normalized to 4pi.
Input is scene-linear, untonemapped outgoing **radiance**. Cosine convolution
uses `A0=pi`, `A1=2pi/3`; stored coefficients are **irradiance divided by pi**.
The lighting shader therefore evaluates SH and multiplies diffuse albedo,
`(1-metalness)`, material AO and indirect strength once, with no additional pi,
exposure or albedo factor. L1 ringing is clamped per interpolated corner.
Valid trilinear weights replace the corresponding global and per-light authored
constant ambient and `sampleEnvironment` diffuse share (panorama or physical
sky). These terms are **replaced, not added** where probes are valid; specular
environment response remains. Invalid/outside weights keep the existing fallback.

GI-03a samples bilinearly filtered distance moments with folded octahedral
seams. The query point is biased **0.05 m along the shading normal and 0.02 m
toward the eye**; SH interpolation coordinates stay unbiased. Each corner's
trilinear weight is multiplied by a smooth wrapped-cosine surface-to-probe
normal weight (`max(0.05, (0.5+0.5*n.d)^2)`) and directional visibility.
For query distance `t`, mean `mu` and variance
`v=max(second-mu²,0.00001 m²)`, Chebyshev visibility is
`(v/(v+max(t-mu,0)²))³`. Cubing reduces the upper bound's leaking tail.
Surviving weights are renormalized, then multiplied by valid trilinear coverage
and the existing volume-edge fade. Rejected corners therefore do not simply
darken a surface; inactive/uncaptured corners retain their fallback share.
If the weight sum is at most 0.000001, coverage becomes zero and the entire
sample uses the existing ambient/environment fallback. Denominators and zero
directions are guarded, including coincident probe/surface positions.

**Inactivation is the chosen inside-geometry strategy; no relocation is used.**
The solid-angle fraction of backface hits must be at most **60%**, and the
fraction with radial distance below **0.10 m near / 0.40 m coarse** (twice that cascade's near plane) at most
**25%**, for SH alpha to become one. Otherwise alpha is zero for that capture.
A point exactly on a large two-sided sheet can see up to half backfaces and
stays active; a closed opaque interior sees almost entirely backfaces and is
inactive. These are raster heuristics, not a watertight inside test for open,
inconsistently wound or sub-near-plane geometry. Inactive probes are revisited
by the existing bounded sweep and may become active after geometry moves.
Untouched rows remain zero; a captured inactive row retains distance metadata
but supplies no irradiance. There is no history accumulation or initialization
flash. `ForwardRenderer::debugProbeVisibility` provides a white-box test baseline
that bypasses only the visibility test, without changing captures, inactivation,
direction weights or public `GiSettings`; applications have no new control.

The fixed-size CPU scheduler shares **B=1 (low) or B=2 (medium)** complete captures
per window frame across both volumes: **near never-captured/exposed first, coarse
never-captured/exposed second, stale near then stale coarse, then globally oldest**.
Missing/stale groups sort by eye distance; cascade and storage row break ties.
A common monotonic clock makes oldest-first refresh fair across both volumes. Inactive captured rows
also age and refresh, so geometry changes can reactivate them. Integration uses
one scissored draw per selected destination row, with its independent capture
atlas slot in the push constant. No contiguous destination-row assumption remains.

| Event | Probe action |
| --- | --- |
| Small snapped camera-cell shift | Preserve overlap; zero only exposed slabs and prioritize them |
| Teleport (snapped near-origin shift greater than one coarse cell on any axis) | Full GPU reset; first frame uses fallback only, updates=0 |
| `clearScene`, successful rendered scene instantiation or `loadScene(Scene)` | Full reset; same fallback-only first frame for an existing volume |
| Successful `removeScene`/`removeSceneInstance`, including when unrelated objects remain | Full reset |
| Direct light/ambient, background, panorama, physical sky/weather changes | Mark captured rows stale; retain last radiance and visibility |
| Cloud animation time, shadow reach or material-lighting mode changes | Mark captured rows stale |
| Object add/remove, material/emission, visibility, shadow, opacity, mesh or transform edits (including scene nodes) | Mark captured rows stale |
| Enable/strength control | Preserve storage/ages; disabled or zero strength pauses captures |
| Quality change | Replace probe storage/sets transactionally; reset validity and convergence |

Invalidation increments a renderer generation at the existing mutation sites;
it neither reuploads scenes nor scans geometry each frame. Each window retains
its own fixed arrays of capture ages/generations and its own atlas. After the
last edit, all **128 rows** refresh within `ceil(128/B)` active window frames:
**128 low / 64 medium frames**. After initial population, a near-cell exposed
slab takes `ceil(16/B)` (**16 low / 8 medium frames**). A coarse-cell move
first repopulates 64 missing near rows, then its 16 exposed coarse rows, taking
`ceil(80/B)` (**80 low / 40 medium frames**), including the crossing frame. Full-reset convergence starts after its fallback-only frame; initial
allocation can start capturing immediately. Repeated edits restart staleness;
disabled, minimized or skipped frames do not advance convergence. Captures
replace old data without temporal accumulation; turning GI off never affects
direct light. Stale values remain available during refresh, avoiding an
initialization black hole or feedback growth.

Capture uniforms and copied scene descriptors are private to each fenced frame
slot and updated only after its fence. Scene buffers/textures are borrowed,
never reuploaded for captures. Persistent images are GPU-serialized on the
shared graphics queue: previous reads precede capture/SH writes; attachment
writes precede integration reads, and SH writes precede lighting reads. No new
steady-state CPU readback, device idle wait or generic render graph is used.
Resize rebuilds screen descriptors at the existing idle boundary; probe images,
capture descriptors, uniforms, validity and ages survive. Probe storage has
fixed size per window and is released through target teardown. Descriptor
layout growth also rebuilds the private capture sets at the existing boundary.
See the consolidated capability requirements below for the additional GI formats/bindings.

Per-cascade payload is **24,576 bytes** RGBA16F radiance/distance + **12,288**
D32 capture depth + **69,632** RGBA32F SH/moments + **24 * sizeof(FrameUniforms)**
fenced face uniforms. `FrameUniforms` stays **8864 bytes**: main depth
reconstruction's matrix slot carries the probe-sun transform only in capture
uniforms, and reserved viewport lanes carry capture sun index / main cascade
count. No frame uniform grows. On the tested Vulkan device, actual VMA
allocations equal this formula: **319232 bytes near**, **319232 coarse**,
**512*512*4 = 1048576 probe-shadow**, total **1687040 bytes/window (1.609 MiB)**,
below the **2097152-byte (2 MiB)** cap. These byte counts depend on the device's memory requirements:
the RTX 3080 Laptop GPU reports **320000 bytes per cascade** and **1688576 bytes total**.
Tests compare allocation accounting with Vulkan's image/buffer memory requirements,
use the payload formula as a lower bound, and retain the same 2 MiB cap.
The internal probe receipt's `gi.gpu_bytes` is the measured fixed total;
`gi.near.gpu_bytes`, `gi.coarse.gpu_bytes`, `gi.probe_shadow.gpu_bytes` give the
measured breakdown. Per-cascade valid/inactive/updated counts come from GPU
alpha/moment readback and actually recorded captures, not constant success
values. Driver-owned descriptor/pipeline memory and the separately reported
screen-sized `ao.gpu_bytes` are excluded. Allocation cost is independent of
resolution; windows do not share volumes or cached probe shadows. GI-07 enforces
the aggregate allocation limit below. Physical-GPU performance is unverified.

### Approximation limits

This is **approximate diffuse GI**: one direct-lit/emissive/sky bounce, represented
by low-order SH with finite raster capture and directional-depth resolution.
The documented local volumes, capture distances, blend bands and cached sun
footprint bound coverage. Thin occluders and unresolved foliage can still leak;
inside-geometry classification is a raster heuristic. VTL captures the first sun
and bounded vegetation LODs. Transparency receives indirect light but contributes
no opaque blocker. Multiple bounces, path-traced specular transport, refraction,
caustics and unlimited-range transport are outside this implementation.

### Performance / Presets (GI-08)

GPU pass timing is an explicit facade diagnostic, independent of renderer selection:

```cpp
render.setGpuTimingReadback(true);
// After ordinary Engine::step calls; this copies receipts and never waits for the GPU.
auto receipt = render.renderDiagnostics(window);
```

The receipt is an owning `Vector<pair<string,string>>`. `gi.capture_ms` sums
the updated cascades' direct-only face captures and publication barriers;
`gi.integrate_ms` sums their SH/visibility integration and publication barriers.
Cached sun/light shadows are outside those intervals. `ao.gpu_ms` includes
evaluation, filtering and publication; `gi.lighting_ms` measures the fullscreen
opaque lighting pass, also on deferred. `gpu.frame_ms` spans the recorded command
buffer, including shadows, native geometry, composition and post-processing/GUI
when present. It excludes CPU preparation, acquire, presentation and PNG capture.
Pass boundaries use bottom-of-pipe timestamps, so opt-in measurements serialize
work and include stores/dependencies; they are diagnostic costs, not a promise of
uninstrumented throughput.

Each window owns 14 queries per frame slot: two capture/integration pairs (one
per cascade), AO, lighting and the whole frame. Query reset/reuse and nonblocking
`WITH_AVAILABILITY` reads occur only after the existing slot fence signals.
Only successfully submitted recordings become pending results. There are no
additional waits, pixel readbacks, steady-state uploads or device-idle calls.
The timestamp-valid-bit mask handles counter wrap. Unsupported queue timestamps
or timestamp period, disabled diagnostics, and not-yet-completed results produce
the literal `unavailable`. Once a timestamped frame completes, an omitted pass
reports **0** because that submission recorded no such work. Receipts describe
the completed submission identified by `gpu.timing_serial`, usually two frames
behind the newest submission; skipped/minimized frames need not publish a new
serial. Disable/re-enable and resize discard the previous receipt. Query pools
remain alive across diagnostic toggles and release at the normal resize/teardown
completion boundary.

`gpu.allocated_bytes` is measured whole-engine VMA allocation bytes;
`gi.gpu_bytes` in this facade receipt sums active and fence-retired GI/AO VMA
allocations across windows. Driver-owned query/descriptor/pipeline allocations
and application-owned native resources are excluded. These are allocation
bytes, not physical memory residency or VMA heap/block reservations.

`HybridGiPerfTests` is registered with `gi`: six compact 160x90 runs test default
unavailable receipts, lazy allocation, real active-pass durations, absent-pass
zeros, increasing retired submission serials, toggles, teardown and unchanged
device-idle counts. Debug requires an active validation messenger. Its
`--measure DIRECTORY` mode runs the same fixed scene at **1280x720**, no-AA and
MSAA4, with **128 warm-up frames and 30 samples** per backend/preset. The scene
has a neutral ground and twelve red/green/neutral cuboids, fixed camera, PBR sun,
ambient, exposure and animation time. AO is enabled for both GI presets;
`--ao-off` provides the independent existing toggle comparison. PNG capture
occurs after measurement. Percentiles use nearest rank: sorted sample 15/29
for median/p95 of 30. Incremental median/p95 subtract the corresponding deferred
statistics; they are not percentiles of paired frame differences.

Measured 31 October 2026 on **AMD Ryzen 7 9700X (8 cores / 16 threads)**,
**llvmpipe (LLVM 20.1.2, 256 bits)**, **Mesa 25.2.8-0ubuntu0.24.04.4**, device
**Vulkan 1.4.318**, Vulkan SDK **1.4.350.1**, Clang/libc++ **18**, Linux Debug
with Khronos synchronization validation active. These are **software-rasterizer
numbers, not physical-GPU numbers**. The bounded diagnostic tried every installed
non-lvp ICD. Radeon reported `Could not open device /dev/dri/renderD128:
Permission denied (VK_ERROR_INCOMPATIBLE_DRIVER)`; the others found no usable
GPU. No permissions or groups were changed. Timing runs used the lvp ICD, X11,
one window and no competing build/test jobs.

| GiQuality | Captures / frame (both volumes combined) | Face | Cascades / cells | Full fresh sweep | Fixed probe/shadow bytes |
| --- | ---: | --- | --- | ---: | ---: |
| low | 1 | 16x16 | 2 x 4x4x4 | 128 active frames | 1,687,040 |
| medium (default) | 2 | 16x16 | 2 x 4x4x4 | 64 active frames | 1,687,040 |

1280x720, AO on for hybrid, identical scene/direct light; milliseconds below are
**median / p95**. Extra VMA bytes are relative to deferred at the same sample
count (deferred totals: **62,558,232 bytes** at 1x, **169,463,832 bytes** at 4x).

| Samples | Preset | Whole GPU command buffer ms | Extra ms vs deferred | Extra VMA bytes |
| --- | --- | ---: | ---: | ---: |
| 1x | deferred | 6.785 / 7.950 | 0 / 0 | 0 |
| 1x | low | 45.447 / 47.552 | 38.662 / 39.602 | 5,373,440 |
| 1x | medium | 46.182 / 47.598 | 39.397 / 39.648 | 5,373,440 |
| 4x | deferred | 28.203 / 31.070 | 0 / 0 | 0 |
| 4x | low | 100.364 / 100.988 | 72.161 / 69.919 | 5,373,440 |
| 4x | medium | 101.503 / 103.493 | 73.301 / 72.423 | 5,373,440 |

| Samples / preset | gi.capture_ms | gi.integrate_ms | ao.gpu_ms | gi.lighting_ms |
| --- | ---: | ---: | ---: | ---: |
| 1x deferred | 0 / 0 | 0 / 0 | 0 / 0 | 4.445 / 4.952 |
| 1x low | 0.167 / 0.229 | 0.993 / 1.051 | 24.053 / 25.413 | 16.883 / 18.177 |
| 1x medium | 0.283 / 0.393 | 1.899 / 1.937 | 23.915 / 25.457 | 16.873 / 18.018 |
| 4x deferred | 0 / 0 | 0 / 0 | 0 / 0 | 17.874 / 19.497 |
| 4x low | 0.162 / 0.235 | 0.971 / 1.013 | 19.297 / 19.599 | 68.795 / 69.680 |
| 4x medium | 0.288 / 0.389 | 1.883 / 1.955 | 19.064 / 19.830 | 68.811 / 70.722 |

Low and medium report identical GPU byte deltas because both allocate the same
grids, face atlases, maximum-budget fenced face uniforms and AO images. Only the
number of captures recorded per frame changes; low does not shrink these allocations.
Low retains its one-capture budget and is the lower-cost refresh preset; medium
converges twice as quickly. No face/grid changes were
justified by software-only measurements. Full-resolution contact AO dominates
1x cost, and per-coverage-sample probe visibility/lighting dominates MSAA4 cost.
Use no-AA and the independent AO-off control for lower software-renderer cost;
AO remains off by default. The existing renderer defaults are unchanged.

The independent AO-off run (same warm-up, 30 samples, scene and device) measured
**20.494 / 22.602 ms** for low and **21.710 / 23.042 ms** for medium at 1x,
against **6.674 / 7.009 ms** deferred: increments **13.820 / 15.592** and
**15.036 / 16.033 ms**. At 4x, low was **79.372 / 81.029 ms**, medium
**80.624 / 82.710 ms**, deferred **28.411 / 30.363 ms**: increments
**50.961 / 50.666** and **52.213 / 52.347 ms**. Both runs retain the same
5,373,440-byte allocation because AO toggles preserve their images/descriptors.
This gives a usable roughly 49 software GPU frames/s for low/no-AA in this
small scene; actual application CPU and presentation costs are additional.

The provisional **+4 ms / +64 MiB at 1080p** target is **not demonstrated**:
even these 720p software timings exceed +4 ms substantially, and no physical
GPU was accessible. The measured 720p increment is **5.12 MiB**. At 1080p the
fixed allocation plus two R16F AO images has a **calculated**, not benchmarked,
payload of **9,981,440 bytes (9.52 MiB)** before any extra device alignment;
this excludes driver-owned objects and deferred infrastructure. It is below
the provisional memory target, but physical-GPU timing/residency and 1080p
benchmark acceptance remain outstanding. Reducing probe updates alone cannot
remove the measured screen-space costs; physical-GPU profiling should precede
changes to AO resolution or the visibility sampler.

#### Actual countryside measurement

The matching VTL Debug `countryside --tessellated` target used the same device,
driver and validation environment, **960x540, no-AA**, seed **12345**, a **1 km
region (8x8 tiles of 128 m)**, the fixed **1.8 m ground camera**, clear preview
weather, frozen animation time **0**, identical sun and tonemap exposure **0.85**.
Both runs rendered **96 actual presented frames**. Statistics select **30 unique
retired submissions 65..94**, after **64 warm-up frames**, allowing two slots
of collection latency. `VTL_GPU_TIMINGS=1` enables these facade receipts for
finite automation; ordinary/interactive launches retain their existing behavior.

Both scenes contained **511 trees, 95,458 grass instances, 516 bushes, 154 stones,
336 flowers and 40 water bodies** (21 visible in this view). The receipt parser
checked identical native asset byte/count fields and zero validation errors.
Final terrain height uploads remained **1**, and native instance upload bytes
were **0** on the stationary final frame. Both final PNGs were inspected:
ground, foliage cutouts, stones/flowers and transparent water remain present;
the existing no-AA foliage stippling is visible. This is a performance fixture;
the independent transport/leak/material tests establish GI correctness.

| Renderer / preset | Capture ms | Integration ms | AO ms | Opaque lighting ms | Whole command buffer ms | Engine VMA bytes |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| deferred | 0 / 0 | 0 / 0 | 0 / 0 | 16.560 / 17.170 | 3286.685 / 3306.104 | 48,041,200 |
| hybrid-gi / medium, AO on | 663.146 / 675.327 | 1.902 / 2.368 | 18.700 / 20.218 | 21.988 / 22.956 | 3972.482 / 3984.801 | 51,801,840 |

The incremental whole-frame cost is **685.797 / 678.697 ms**, with
**3,760,640 extra VMA bytes (3.59 MiB)**. Application-owned native allocations
and driver-owned objects are outside these VMA totals; existing native asset
byte receipts are retained separately in each log. The 30-frame window starts
with near-volume refresh. Coarse-volume warm-up captures were more expensive:
all recorded capture intervals ranged from **647.178 to 2271.284 ms**. Keep
that update-phase dependence when interpreting this small sample. Most base
frame time is outside the measured fullscreen lighting pass, in the retained
native geometry/shadow work; GI adds substantial native capture work on this
software device. These results meet neither the provisional timing goal nor
physical-GPU verification. They preserve the existing countryside content and
the bounded one/two-probe update presets instead of reducing scene density for
the comparison.

Reproduce with the SDK layer/library environment from the verification section
below and a working presentation driver. Select the intended ICD explicitly;
the recorded runs use `/usr/share/vulkan/icd.d/lvp_icd.json`. AI-loop runs these
commands through its crash-safe wrapper with a bounded 1800-second timeout.

```sh
# VVE worktree root; report is an absolute directory outside either repository.
report=/tmp/ai-loop-worker-reports/J20261030-180654-207139/gi08
cmake --preset debug-linux && cmake --build --preset build-debug-linux
bin/debug/exe/HybridGiPerfTests --measure "$report/vve" > "$report/vve-720p.log" 2>&1
bin/debug/exe/HybridGiPerfTests --measure "$report/vve-ao-off" --ao-off > "$report/vve-720p-ao-off.log" 2>&1
# From the matching VTL root after building its countryside target:
for backend in deferred hybrid-gi; do
  VTL_GPU_TIMINGS=1 bin/debug/exe/countryside --tessellated --seed 12345 \
    --region 8x8 --tile 128 --camera-height 1.8 --renderer "$backend" \
    --gi on --ao on --frames 96 --output "$report/countryside-$backend.png" \
    > "$report/countryside-$backend.log" 2>&1
done
```

The retained GI-08 artifacts use `vve-720p-final.log` and
`vve-720p-ao-off-final.log`, six PNGs in each VVE directory, the raw countryside
logs/PNGs, parsed `vve-summary.json` / `countryside-summary.json`, device denial
and CPU logs, configure/build/CTest logs and `commands.txt` under that report
directory. The compact CTest path writes no new PNGs.

The GI-08 benchmark recorded zero added device-idle waits. Current configure/build
and regression results are consolidated in GI-09 below; the performance tables
and their original software-device provenance remain unchanged.

### Lifecycle, ownership and memory limits (GI-07)

Every rendered window owns its G-buffer, depth/HDR/MSAA attachments, raw/filtered
AO images, opt-in GPU query pool, two probe volumes and cached probe-sun shadow.
Even nearby windows have independent origins, validity, ages and reset counters;
there is no shared history. AO uses spatial filtering and has no temporal history.
Minimized/zero-extent windows skip submissions and retain all owners and camera
overrides. Resume continues convergence. Resize replaces only screen-sized
attachments and their sampled set at the existing resize idle boundary; it
preserves probe images, capture sets, uniforms and validity. No pipeline rebuild
or probe reset is caused by resize alone.

The per-window fixed-probe limit is **2 MiB** (measured **1,687,040 bytes** on
llvmpipe). Probes plus AO are limited to **32 MiB per window** and **64 MiB per
engine**, including all fence-retired generations. These are VMA allocation
bytes, including alignment; driver-owned descriptors, queries and pipelines,
application-owned native resources, and existing deferred G-buffer/HDR/depth
infrastructure are excluded.
AO is two R16F images, about **8,294,400 bytes at 1080p** before alignment.
Large extents or too many distant windows fail initialization with
`platform_error`; quality requests that exceed the total reservation return
`capacity_exceeded` and preserve settings and resources. Windows are never
silently downgraded. Allocation failures release partial owners before retry.

Runtime low/medium changes allocate fresh volumes, face uniforms, capture sets,
probe shadow and sampled descriptors for **every window**, committing only
after all allocations succeed. Screen images and AO remain unchanged. Previous
owners join the existing graphics-submission retirement queue; completed frame
slot fences reclaim them without device idle, readback or scene reupload.
Identical-quality calls and enable/strength/AO toggles do not allocate.
Both presets intentionally have equal storage sizes and coverage, with one/two
captures per active window frame. A quality change clears validity and begins
fresh convergence (**128 low / 64 medium frames**); initial replacement may
capture immediately. Uncaptured rows use environment fallback.

The [event table above](#approximate-diffuse-gi-gi-02) defines scene/teleport resets,
stale refresh and their convergence bounds; none of those events reallocates images.
`gi.history_resets` counts actual reset recordings once per window frame for
either/both volumes, including initial/quality allocation and whole-volume
scroll resets. Skipped acquisitions and minimized frames do not increment it.
Ordinary lighting/geometry edits retain stale probes and refresh within the
same documented budget. Native callback replacement/clearing retires captured
application owners by submission fences and invalidates radiance/caster depth;
the next prepare callback receives the current borrowed views after replacement.
Renderer shutdown, including failed initialization cleanup, clears the native
recorder before destroying the device; applications re-register it when retrying.

Opt-in native diagnostics (`VVE.TestSupport`/lifecycle receipts) report
`gi.gpu_bytes` (fixed probes/shadow), `ao.gpu_bytes`, `gi.window_gpu_bytes`
(their sum), and `gi.total_gpu_bytes` (all active and retired GI allocations).
`gpu.live_allocations` queries actual VMA allocations for the whole renderer,
including shared infrastructure; `gpu.cleanup_live_allocations` measures them
immediately before allocator destruction. The internal one-shot failure
checkpoint is absent from the public facade. Ordinary frames perform none of
these diagnostic queries or CPU readbacks.

`HybridGiLifecycleTests` plus its six cases cover distant windows/cameras,
1x/MSAA4 resize, minimize/resume, isolated teleports, five engine cycles, partial
probe/AO/descriptor and second-window initialization failures with clean retries,
quality rollback in the second window after a first-window exchange, rapid
quality changes up to the aggregate limit, and scene/native-owner replacement.
Tests require Debug validation, emit measured allocation/reset receipts and
check validation through teardown; missing GPU/validation is a failure.

The GI-07 Linux Debug lifecycle run passed **7/7** cases on **llvmpipe (LLVM
20.1.2, 256 bits), Mesa 25.2.8, Vulkan 1.4.318**, with Khronos synchronization
validation and **zero errors through teardown**. Five cycles each retained
**75 live VMA allocations**, then measured **zero before allocator destruction**.
Fixed probes/shadow measured **1,687,040 bytes/window**; 64x64 and 96x80 AO
measured **16,384 / 30,720 bytes**, for **3,421,184 bytes** combined across two
windows. Resizing the first to 112x88 changed only its AO to **45,056 bytes**,
for **3,449,856 bytes** combined. Eighteen rapid quality replacements reached
**64,154,624 bytes including retired generations**, below the **67,108,864-byte**
engine limit; the next request failed transactionally. After fences, allocations
returned to baseline. Quality changes added **zero device-idle waits** and no
mesh or texture uploads. These are software-Vulkan correctness/memory results,
not physical-GPU timing measurements.

### Capture shadows and coarse cascade tests (GI-05b)

`HybridGiCascadeTests` and six registered subcommands (`coarse-bounce`,
`continuity`, `coarse-scroll`, `teleport`, `memory`, `memory-low`) run serially
with `gpu;gi` labels, validation failure expressions, fixed 128x128 views,
paused animation and native linear FP16 HDR region readback. Memory/convergence
frames are measured from empty storage, not presumed from warm-up length.
PNG capture acquires another frame, so teleport reads HDR **before** requesting
any PNG or subsequent frame. Images remain supplementary to linear receipts.

| Fixture | Gate and derivation |
| --- | --- |
| Shadowed capture | The off-frustum red wall's measurement point lies outside all four main directional projections. A black roof shades it from the sun. Roofed/unroofed receiver red-minus-green bounce must be below the main-view direct wall reference fraction **+0.15**, itself below 0.65. The main reference retains the existing 0.35 shadow floor; 0.15 allows finite 16x16 integration and the probe-map penumbra. Unroofed bounce must exceed 0.005. Main-view deferred vs GI-disabled wall HDR error is **exactly 0** for roof on/off. Cache counters also test unchanged frames, intensity, sky, color and roughness versus sun direction/caster motion. |
| Coarse bounce | An 11.10 m distant receiver at z=-8 is outside near coverage, inside coarse. Red-minus-green and recolored green-minus-red must each exceed **0.005** (the existing color-transport gate). Disabling coarse only through the internal test hook restores GI-off fallback with **exactly 0** HDR error. |
| Continuity | 81 samples at **0.25 m** spacing cross near and coarse boundaries, on a uniform floor. Largest adjacent step is below **2 * abs(first-last) * 0.25 / 2 + 0.002**: twice the linear endpoint slope over the narrowest 2 m band, plus the existing 0.002 FP16/quadrature tolerance. Every sample stays at least `min(endpoints)-0.002`, finite and above 0.001 (no black holes). |
| Coarse scrolling | One 8 m move retains **48** coarse rows with **byte-identical** SH/moments and ages. Near's 64 missing rows plus coarse's 16 exposed rows recapture within **ceil(80/B)** frames. |
| Teleport | Both GPU validity/moment images are empty and updates zero on the first frame. Receiver HDR equals explicit disabled fallback with **exactly 0** error; both grids subsequently converge. |
| Memory / low | Actual allocation sum matches Vulkan's image/buffer memory requirements, is at least the payload formula above, and stays **<=2097152**. Both 64-row grids have measured GPU validity and converge within **64 medium / 128 low** frames; every frame records at most the shared budget. |


GI-05b Linux Debug verification on 30 October 2026 used Clang/libc++ 18,
Vulkan SDK 1.4.350.1, and **llvmpipe (LLVM 20.1.2, 256 bits)** with Khronos
validation and synchronization validation: **47/47 GI** (40 existing + 7 new)
and **31/31 deferred** passed, with zero validation errors. Measured receipts:

```text
shadow.reference_fraction=0.3507899
shadow.allowed_fraction=0.5007899
shadow.off_view_fraction=0.35151765
shadow.main_view.delta=0
coarse.red_shift=0.5038147
coarse.green_shift=0.5038147
coarse.fallback.delta=0
continuity.max_step=0.0026397705
continuity.step_limit=0.01448169
continuity.minimum=0.049987793
continuity.minimum_limit=0.047987793
scroll.coarse.overlap=48
scroll.convergence.frames=40
teleport.near.valid=0
teleport.coarse.valid=0
teleport.fallback.delta=0
gi.cascades=2
gi.near.gpu_bytes=319232
gi.coarse.gpu_bytes=319232
gi.probe_shadow.gpu_bytes=1048576
gi.gpu_bytes=1687040
gi.convergence.medium.frames=64
gi.convergence.low.frames=128
validation.errors=0
```

Disabled/zero-strength, AO-off equivalence differences stayed **0** at no-AA
and MSAA4; all six deferred/disabled/zero PNGs also matched saved pre-task
captures byte for byte. Roof, coarse red/green and continuity captures were
opened and inspected. These are software Vulkan correctness results; physical
GPU performance, Windows/macOS and VTL integration are outside this verification.

### Material transport and transparency (GI-05a)

The glTF importer retains `alphaCutoff` and `doubleSided` in the concrete material.
Hybrid G-buffer, capture and shadow fragments share `materialBaseColor` and
`materialAlphaMask`: discard when factor alpha times texture alpha is **less than**
the imported cutoff (default `VVE_ALPHA_CUTOFF=0.5` in `simple_shared.h`). Equality
is opaque. Discard removes both radiance and distance/depth, including in shadows.
The hybrid shadow pipeline adds UV interpolation and an alpha-only fragment stage.
Existing forward/deferred shadow and material policies remain unchanged.

Hybrid geometry pipelines disable hardware back-face culling, then discard
single-sided back faces in main-view material fragments; double-sided cards use
the common normal orientation and material maps on either face. Capture retains
signed back-face distance even for single-sided solids, because inside-probe
inactivation depends on those hits. Their back-face RGB is zero; double-sided
foliage reflects direct light/emission from either face. Thus reversing a red
single-sided card away from the receiver reduces bounce, while a double-sided
card still bounces. Closed-solid interiors retain their existing validity test.
This explicit policy belongs to imported materials. Engine-authored materials
have no sidedness setting in the facade and retain their original back-culled
main view and bidirectional capture response; changing that default also changes
the existing scrolling/sky baseline. The concrete optional sidedness and shared
GPU policy constants distinguish absent metadata from an explicit single side.
Blended, object-opacity-transparent and sprite draws are excluded from **both**
capture radiance and depth/moments, regardless of their main-view visibility.
Transparent water is not converted into an opaque capture blocker.

The public irradiance sampler described above is shared by deferred lighting and
the hybrid-only forward transparent fragment variant. Transparency reads the same window's
SH/visibility atlas using the existing deferred descriptor layout; opaque depth
returns to its attachment layout first. The blended pipeline keeps depth tests,
sorted composition and no depth writes. Transparency receives material AO, **no
screen-space AO**. Linear HDR lighting precedes the existing post-processing,
tonemapping and GUI; no display-space GI pass or extra history is introduced.

Let `C` be valid coverage including the edge fade, `P` the visibility-normalized
probe E/pi, `F` the old diffuse environment response and `s` indirect strength.
At strength one the diffuse response is `C*P + (1-C)*F`; for other strengths it is
`F + s*C*(P-F)`. Both the added probe term and the removed fallback share scale
with strength. This makes the GI-on minus GI-off delta linear in `s`, including
nonzero authored ambient. Values above one extrapolate this artistic blend.
Probe diffuse multiplies linear base color and `(1-metalness)` once. Classic
authored ambient predates the metal rule: its existing metallic share is retained,
so replacing diffuse cannot remove a metal's old constant contribution. The
environment specular term, direct-light BRDF and emission are independent of GI.
All material views use the same texture table: base/emission textures decode
sRGB in hardware; factors and normal/roughness/metalness/AO maps remain linear.

`HybridGiMaterialTests` registers cutout, cutout-MSAA4, double-sided, metal,
strength and sRGB cases; `HybridGiTransparencyTests` registers color,
color-MSAA4, blocking, ordering and tonemap cases. Every case is `gpu;gi`, serial,
fails on validation errors/VUIDs, prints the device/regions/limits and checks
validation through teardown. They use 128x128 scenes, fixed animation, no camera
fog, 64 warm-up frames (one combined medium-budget sweep) and native FP16 HDR readback.
PNGs and generated glTF fixtures live under `build/debug-linux/tests/tmp/` in
the corresponding executable/subcommand directory. All thresholds are linear
HDR maximum RGB errors unless explicitly stated otherwise:

| Check | Threshold and derivation |
| --- | --- |
| Cutout bounce, no-AA/MSAA4 | 2048/4096 checker texels are below cutoff 0.7 (alpha 153 vs 255). `(masked-empty)/(solid-empty)` must be 0.30..0.70: 50% coverage with a 20 percentage-point allowance for 16x16 cubemap quadrature, directional moments and L1 interpolation. Solid bounce must exceed 0.005; empty/solid limits cannot pass. |
| Main/shadow holes | Main hole matches the no-card floor within 0.005; a solid texel is red. Shadow hole is lit above 0.1, while masked solid texel and full-card shadow are each below 80% of that value. Regions lie inside checker cells, away from PCF boundaries. |
| Reversed card | Double-sided minus single-sided red exceeds 0.005; single-sided bounce retains less than half the double-sided bounce. A separate main view must show the double-sided back at least 0.1 brighter than the culled single-sided back. |
| Metals | Metalness 1 GI delta below 0.002; metalness 0.5 delta within 0.002 of half the dielectric delta, at the identical position. Dielectric delta exceeds 0.005. A simultaneous adjacent dielectric/metal pair checks the same direction. Nonzero 0.12 authored ambient catches accidental removal of metallic fallback. |
| Strength | Half-strength delta within 0.001 of half the full delta (several FP16 rounding steps); full delta exceeds 0.005. Sun-lit open reference outside coverage and pure-emission reference each change by less than 0.0005. |
| sRGB | Texture bytes (128,32,32) decode analytically to factors (0.21586053,0.014443844,0.014443844). Bounce mismatch below 0.001 and main wall mismatch below 0.003 allow hardware sRGB conversion and FP16 lighting rounding. Bounce exceeds the no-wall value by 0.003. |
| Transparent color/AO | Pane over black unlit floor has red/green deltas above 0.003 with dominant-channel ratio above 1.5 following wall recoloring. Half-strength error below 0.001; toggling opaque SSAO changes the pane by less than 0.0005. |
| Transparent blocker | Red-minus-black contributor bounce retains at least 95% of no-pane bounce (5% numerical margin); identical opaque pane retains less than 30%. No-pane bounce exceeds 0.005. |
| Depth order | Adding a hidden transparent pane changes the opaque region by exactly zero; the unlit reference matches within 0.001. Recorded geometry, lighting and transparency passes must occur in order. |
| Before tonemapping | At exposures 0.5 and 2, HDR radiance changes by less than 0.0005, while decoded PNG follows `ACES(exposure*HDR)` within 0.008 (at most roughly two sRGB byte steps plus FP16). Both GI states are checked; GI changes HDR and display by more than 0.003, and transparency precedes the post pass. |


Linux GI-05a verification used Clang/libc++ 18, Vulkan SDK 1.4.350.1 and
**llvmpipe (LLVM 20.1.2, 256 bits)**, Vulkan 1.4.318, with Khronos validation
and synchronization validation active: **40/40 GI** (29 existing + 11 new) and
**31/31 deferred** checks passed, with zero validation errors. Cutout retained
0.54764/0.54773 of solid bounce at no-AA/MSAA4; main holes differed by zero and
shadow holes measured 0.58984 vs 0.26489 under solid texels. Full-metal delta
was zero; half-metal error was 0.0000153. Strength error was 0.0000611 with zero
sun/emission deltas. sRGB bounce/main errors were 0.000305/0.002930.
Transparent red/green dominant deltas were 0.12173, AO delta zero, and its
blocking ratio was 1.0 vs 0.000640 for the opaque control. Opaque ordering and
HDR exposure deltas were zero; maximum ACES error was 0.000230. Both disabled
and zero-strength equivalence differences remained zero, and all six deferred/
disabled/zero no-AA/MSAA4 PNGs matched the saved pre-task files byte for byte.
Cutout, shadow, reversed-card, metal, transparency and exposure PNGs were opened
and inspected. Physical GPU device access was denied; no hardware performance,
VTL participation, Windows or macOS result is claimed by this verification.

### Spatial contact AO (GI-04)

With `GiSettings::ao_enabled=true`, two fullscreen passes run **after G-buffer
publication and before deferred lighting**, only in `hybrid_gi`. Evaluation
reconstructs view-space positions in metres from D32 and rotates G-buffer world
normals into view space. Eight azimuths, each with four radial taps, search a
**0.5 m world-space radius**. Each azimuth retains the largest positive normal
elevation above a **0.02 m tangent-plane bias**, weighted by
`1-distance²/radius²`; visibility is one minus their mean. Distant layers and
sky are rejected. A fixed 4x4 interleaved golden-ratio rotation varies azimuth
per pixel; it uses neither random seeds nor animation time.

Both raw and filtered images are **full drawable resolution, single-sample
R16F**, so there is no upsampling. A single **5x5 bilateral filter** uses spatial
Gaussian weights `exp(-(x²+y²)/4)`, linear-depth weights `1-|dz|/0.05 m`, and
normal weights `max(dot(n,nq),0)^8`. It rejects sky, depth gaps at least 0.05 m
and normal dot products below 0.9, preserving silhouettes. The world-radius
test in evaluation also rejects a foreground object as an occluder of a distant
wall. Constants needed across renderer/shader code live in `simple_shared.h`.

Under **MSAA4**, depth and normal come only from **coverage sample 0**, with its
ray approximated by the pixel centre (the 2 cm evaluation bias absorbs subpixel
position error). No depth or normal average mixes unrelated surfaces. Lighting
still runs per coverage sample. It uses that pixel's AO only when source linear
view depth differs by at most **1% of the lit sample's depth**, and world normal
dot product is at least 0.9. Otherwise it searches the **four direct neighbors**
for the closest depth within the same tolerance and normal gate; no match gives
visibility 1.0. This also protects a wall sample in a partially covered pixel.

AO attenuates **only diffuse indirect light**: probe irradiance and the uncovered
`(1-coverage)` authored ambient / environment diffuse fallback. It is independent
of probe enablement and strength. The effective diffuse visibility is
`min(material_ao, ssao)`, rather than their product, because both approximate
overlapping occluders and multiplication double-darkens contacts. Diffuse probe
and environment terms retain their `(1-metalness)` factor. Direct lights,
emission/unlit output, sky, fog, transparent objects/water, post-processing, GUI
and environment specular response retain their existing shading. With AO off
neither pass draws, no AO image is sampled, and the prior lighting expressions
are used in the original compiled lighting variant, preserving active-probe
evaluation order and disabled/zero-strength equivalence with deferred. Runtime
toggles select the retained pipeline without recompilation or descriptor changes.

The two images cost **4 bytes per drawable pixel** before allocation alignment
(7.91 MiB at 1920x1080), allocated per hybrid window even while AO is disabled
to make toggles descriptor-stable. `ao.gpu_bytes` reports their **actual VMA
allocation sizes** per window; it excludes driver-owned pipelines/descriptors
and the optional query pool. Near probe storage remains fixed across resize, with its byte count depending on device allocation requirements; the GI-05b combined
volume/probe-shadow budget is documented above.
`ao.enabled` records whether passes actually ran. With `setGpuTimingReadback(true)`,
`ao.gpu_ms` reports the latest fence-completed evaluation/filter timestamp interval,
or **unavailable** without supported timestamps or before completion. A completed
timestamped frame with AO off reports zero recorded AO work. Query slots reset
only after their existing frame fence, with no blocking query read; see GI-08 above.

Images are reused in graphics-queue order: G-buffer writes precede AO reads,
raw attachment writes precede filter reads, filtered writes precede lighting
reads, and prior reads finish before next-frame overwrites. Resize recreates
the images/descriptors at the existing idle boundary; teardown and failed
allocation release partial storage. Toggling adds no device-idle wait, resource
replacement, scene upload or steady-state CPU image readback. There is **no
temporal history**: the deterministic spatial kernel is sufficient for this
contact-scale effect and needs no reprojection, disocclusion or camera-cut
history. Every enabled frame recomputes the complete image.

`HybridGiAoTests` and its registered `gi` subcommands cover ambient/probe
contact light, open and sun-lit ground, unlit/emissive/sky references, a floating
cube against a distant wall, MSAA4 contact and silhouettes, imported material AO,
resize and off/on/off HDR bit equality. A known grey floor contact must lose
**>10% of indirect light**, well above FP16 quantization; open and sun-lit patches
and all halo/silhouette pixels must change by **<0.002 linear units**.
Unlit/emissive/sky and the material-min prediction use **<0.0005**. Sun subtraction
also checks unchanged direct response at the contact. The imported **64/255** AO
texture is stronger than contact SSAO and must remain unchanged, rejecting a
stacked product. Receipts include explicit HDR region values, actual allocation
bytes, optional GPU timing, device and validation errors through teardown.

`HybridGiBounceTests` and `HybridGiOffCameraTests` use a paused 128x128 fixture:
a horizontal white light illuminates a saturated wall, and has zero direct
cosine to the grey receiver. Black sky and zero authored ambient isolate the
bounce. Explicit HDR receiver regions are read back only at checkpoints, with
64 warm-up frames and two later observations 32 frames apart. Red/green must
rise by >0.5 over disabled GI, the green-wall green/red ratio must exceed 1.5,
and the contributing channel must rise by >0.005 linear units. Removing the
wall must return within 0.003; later observations must agree within 0.002.
These thresholds measure directional color transport, not changed pixel counts.
Captures are retained beside the existing native fixtures. The off-camera case
narrows the view so the wall is outside the main frustum, while capture culling
still sees it. `HybridGiSettingsTests` checks malformed controls and both live
budgets. Equivalence tests compare disabled and zero-strength GI to deferred at
both sample counts with the existing 3-byte full/surface mean-error tolerance.
The active-bounce fixture also runs with MSAA4.

`HybridGiSkyTests` uses a uniform blue panorama, an open grey horizontal
receiver, no direct lights, zero authored ambient and a black background.
After two complete sweeps it prints GI-off/on linear receiver RGB, requires
every probe valid, checks the sky hue and limits per-channel relative energy
error to 25%. Constant radiance is exactly L0; an upper-hemisphere horizon
step evaluates to 1.25 times its exact cosine integral at L1, motivating the
tolerance for this fixture. The existing physical-sky environment uses a
six-step zenith/horizon approximation, while the visible sky uses directional
16-step scattering; their energies differ, so the physical-sky check prints
both paths and additionally compares against an independent L1 cosine integral
of six ordinary main-view sky renders. Subtracting a black receiver removes
unchanged specular reflection and air scattering; dividing by grey albedo gives
E/pi. The 3% reference tolerance covers FP16, subpixel sky filtering and small
main-view/probe air/height differences. A sweep captured with camera haze must
also leave probe light unchanged when the main view is unfogged (0.002 linear
maximum-channel difference).
`HybridGiEmissiveTests` uses the same receiver with a red emissive panel and no
sky or direct lights. The red channel must rise by >0.005 with a red/green ratio
>1.5; replacing its emission with zero while keeping the panel geometry must
return to GI-off within 0.003. Both tests retain paired PNGs and report the
actual Vulkan device, validation state and errors through teardown.

`HybridGiLeakTests` places a black separating wall at x=-1, with grid probes
on both sides, a red lit contributor at x=-3 and neutral horizontal receiver
regions. A horizontal direct light has zero receiver cosine. The protected
region and the open region are each four by four HDR pixels projected from
known surface points, with fixed cameras/exposure/time. The same captured
scene is measured with directional visibility disabled/enabled, then the
contributor is made black and both baselines are remeasured after two sweeps.
Each reported red bounce subtracts its matching no-contributor region, so
neutral/specular response cannot masquerade as leakage suppression. The test
requires a baseline bounce above 0.005 and at least **70% rejection** through
the wall (retained fraction <0.30), plus an unprotected red bounce >0.005 and
red/green >1.5. This isolates the cross-wall corner's transport while allowing
the finite directional filter's tail; a uniform darkening or disabled GI
cannot satisfy the open-side gate.

`HybridGiInsideGeometryTests` runs the same native executable with `--inside`.
A black block encloses four grid points; an adjacent grey receiver at x=-0.9
has 45% trilinear weight on these inactive corners. GI-off fallback is 0.1
linear units, with matching 0.2 background radiance and authored ambient.
The allowed **0.02 linear** error permits real sky occlusion by the black block
in the remaining probes; losing the inactive corners' 0.045 fallback share
would fail. The test also rejects overshoot above fallback+0.02, nonfinite
receiver values and nonfinite SH/moment storage. A bounded opt-in RGBA32F
readback reports actual active/inactive counts from GPU alpha and captured
moment metadata (`gi.probes.valid`, `gi.probes.inactive`), rather than the CPU
`probes.valid` sweep counter, which counts captured rows including inactive
ones. Both tests report `gi.gpu_bytes`, `gi.probes.updated`, receiver regions,
`gi.storage.finite`, the device name and `validation.errors` through teardown.
Their paired PNGs are under
`build/debug-linux/tests/tmp/HybridGiLeakTests/` and are supplementary evidence
to the linear measurements.

`HybridGiDynamicsTests` and its registered `sun-low`, `scroll`, `scroll-low`,
`sky`, `move`, `teleport` and `replacement` subcommands use the horizontal-sun,
red-wall/neutral-receiver fixture with fixed animation and HDR point regions.
Its dim neutral sky keeps the no-wall reference above black; a 0.25-radian
camera measures a four-pixel-wide footprint centered at the same world point
on the shared covered boundary, avoiding changes from the outer volume fade.
Both scroll directions/budgets require at least 48 GPU-valid overlapping rows
on the crossing frame, receiver drift below 0.002 and all 16 exposed rows
captured within the slab bound. Light, panorama-sky and wall-transform edits
check the exact stale-count countdown and finite intermediate HDR values, then
compare with an independently cleared/warmed volume (0.002 maximum-channel
tolerance). Wall movement must also agree with no wall within 0.003. Teleport
and scene replacement require zero GPU-valid and inactive rows on their reset
frame and fallback agreement within 0.002; green replacement must remove the
previous red hue. GPU readbacks remain opt-in test checkpoints only.

Dynamics receipts report stale/valid/inactive probe counts, actual captures and
per-event convergence bounds and HDR deltas. GPU validity counts active alpha;
the CPU captured-row count also includes inactive rows. Allocation and reset
receipt meanings follow the lifecycle contract above.

```sh
cmake --preset debug-linux && cmake --build --preset build-debug-linux
ctest --test-dir build/debug-linux -L gi -N
ctest --test-dir build/debug-linux -L gi --output-on-failure
ctest --test-dir build/debug-linux -L deferred --output-on-failure
```

GI-02 Debug verification on 30 October 2026 used Clang/libc++ 18, the Vulkan
1.4.350.1 SDK validation layer with synchronization validation, and llvmpipe
(LLVM 20.1.2). All 9 GI and 31 deferred tests passed, with zero validation errors.
Disabled/zero-strength comparisons had zero mean pixel difference for both
coverage counts. Receiver linear RGB was `(0,0,0)` disabled,
`(0.3557529,0.0245656,0.0245656)` with the red wall and
`(0.0245656,0.3557529,0.0245656)` with the green wall. Removing the wall returned
to zero; later-frame drift was zero. The verified off-camera red/green ratio
was 14.368226, and active MSAA4 was 14.482189. The paired red/green, off-camera
and MSAA captures were opened and inspected. These are software Vulkan
correctness results; physical-GPU performance and other platforms remain unmeasured.

The GI-02 sky repair was verified on the same Linux software Vulkan device and
SDK validation setup: **11/11 GI and 31/31 deferred tests passed**, with zero
validation errors and unchanged zero-difference disabled/zero-strength results.
The panorama receiver changed from `(0.0858335,0.2008457,0.4500084)` to
`(0.0901880,0.2110786,0.4729919)` (5.11% maximum channel error, blue dominant).
Physical-sky E/pi agreed with the rendered-sky reference within 0.089%; camera
haze contamination was zero. Removing the haze fix makes that regression fail
with a 1.20829 linear error. The emissive panel changed a black receiver to
`(0.3127613,0.0031262,0.0031262)` and zero emission returned exactly to black.
Authored ambient changed the valid-probe result by only 0.00000191 linear units.
The paired sky/emissive captures were opened and inspected; no hardware-GPU
verification is claimed (the AMD device node denied access).

GI-03a's 30 October 2026 Debug run passed 13/13 GI and 31/31 deferred tests
on the same validation-enabled llvmpipe setup. Its measured near-only receipts were:

```text
leak.baseline.red=0.061157227
leak.visibility.red=0.0002822876
leak.retained_fraction=0.0046157683  # 99.54% rejected; limit <0.30
unprotected.bounce.red=0.18642902
gi.probes.valid=60
gi.probes.inactive=4
inside.fallback=0.099975586
inside.receiver=0.09106064
inside.tolerance=0.02
gi.gpu_bytes=319232
gi.storage.finite=1
validation.errors=0
```

The wall fixture reported 64 valid and zero inactive probes; its PNGs were inspected.

GI-03b's 30 October 2026 Debug run passed 21/21 GI and 31/31 deferred tests
on validation-enabled llvmpipe, retaining exact disabled equivalence and the
existing transport/leakage gates. Its measured near-only dynamics receipts were:

```text
scroll.medium.first_valid=50  # 48 preserved + 2 newly captured
scroll.low.first_valid=49     # 48 preserved + 1 newly captured
scroll.receiver.delta=0.0015411377  # both directions; limit <0.002
scroll.medium.convergence.frames=8
scroll.low.convergence.frames=16
sun.medium.convergence.frames=32
sun.low.convergence.frames=64
sun.receiver.red.before=0.364563
sun.receiver.red.after=0.09738159
sky.convergence.frames=32
sky.receiver.blue.before=0.03237152
sky.receiver.blue.after=0.3784637
move.convergence.frames=32
move.receiver.red.after=0.009994507
move.no_wall.delta=0
sun.reference.delta=0
sky.reference.delta=0
move.reference.delta=0
gi.probes.stale.first_medium=62
gi.probes.stale.converged=0
teleport.reset.valid=0
teleport.reset.fallback.delta=0
replacement.reset.valid=0
replacement.reference.delta=0
replacement.receiver.linear=0.032485962,0.36468506,0.032485962
gi.gpu_bytes=319232
validation.errors=0
```

GI-05b subsequently added the coarse volume and independent probe-sun shadow.
Current reset behavior and combined convergence bounds are specified above.

## GI-09 Linux acceptance (31 October 2026)

Device: **llvmpipe (LLVM 20.1.2, 256 bits)**, Mesa **25.2.8-0ubuntu0.24.04.4**,
Vulkan **1.4.318**, Ryzen 7 9700X, matching Clang/libc++ **18.1.3**, CMake 3.31.8,
Ninja 1.11.1 and SDK **1.4.350.1**. These are software-Vulkan correctness results.
Physical GPU access remains denied; Windows/macOS and physical-GPU performance
were not tested. GI-08's physical timing target remains outstanding.

The **actual VTL countryside executable** produced **20 inspected PNGs and 20
matching linear RGBA32F snapshots**: five cameras times all four independent
GI/AO combinations. Seed **42**, clear weather, animation **0**, exposure **0.85**,
**640x360/no-AA**, medium GI and **66 presented warm-up frames** were fixed.
The source is 2x2 128 m tiles at 1 m spacing, `ridgedRelief=false`, with the
existing hydrology parameter `rivers.channelThreshold=4096`; it yields five
water bodies. No generation, collision, hydrology or plant-growth code changed.
The ground preset uses the same **1.8 m bicubic clamp as G**; the other presets
select existing tree/stone/water targets, with a higher overview about 12 m above
the tree root. Eye/target values are retained in each receipt.

Explicit inspected surface rectangles, top-left `(x,y,width,height)`; `Y` is
linear Rec.709 luminance before exposure/tonemapping. AO delta is AO-on minus
off with GI on. Values measure composited native surfaces, including the bed
visible through transparent water.

| View / surface | Rectangle | Y, GI off | Y, GI on | AO delta | GI mean absolute RGB delta |
| --- | --- | ---: | ---: | ---: | ---: |
| Ground / canopy ground | 310,205,25,20 | .1242803 | .1199369 | -.0021701 | .0030721 |
| Forest edge / foliage | 220,270,40,35 | .2411268 | .2422002 | -.0000546 | .0014583 |
| Details / stone | 313,165,15,25 | .1464765 | .1505202 | -.0078363 | .0126041 |
| Water / surface | 400,270,60,35 | .1204576 | .1200221 | -.0000163 | .0005085 |
| Overview / canopy ground | 275,205,35,30 | .1118943 | .1037467 | -.0023552 | .0061548 |

Occluded canopy/stone regions lose sky/fallback energy; foliage and the detail
stone gain indirect light, with the stone's green channel rising **.0051494**.
The shallow underwater-bed region `(170,295,40,30)` gains **.0012631 Y**. These
comparisons complement the registered native-ground transport/removal test;
changed pixels alone are not the evidence for a diffuse bounce.
That independent fixture measured **.0545678 green radiance**, green/red **1.39018**
from native ground outside the main view, with **zero error after removal**.

All **19 explicit region gates** pass: the existing native-fixture **1e-5 linear
signal floor**, negative contact-AO energy, and **.002 linear** open-reference
stability tolerance. Ground's open reference `(570,140,25,12)` has **exactly zero
GI error** and **.0002176 mean RGB AO change**; forest/water/overview outside-volume
references also have zero GI error. Their largest mean AO change is **.0014053**,
versus **.0076972** at the detail stone contact. Maximum AO brightening in every
measured region is **zero**. Engine AO/material tests independently isolate
unchanged direct sun, emission and sky. Full HDR and per-state RGB values remain
available for narrower regions; grass/foliage silhouettes are not treated as one
averaged normal or depth.

Water is present and visibly transmits the bed in all four water captures:
**312 transparent draws**, **zero opaque/G-buffer/shadow/probe water draws**.
Its small composited AO delta comes from the visible opaque bed; water shading
does not sample contact AO.
The independent red/blue-bed transmission fixture measured **.270752** of either
bed signal, verifying alpha composition.
Native terrain, trunks, foliage, shrubs, stones, flowers and grass all record
probe-capture draws. The final stationary native instance-upload count is zero.
Measured engine GI/AO allocation is **2,608,640 bytes at 640x360**. These capture
runs overlap other verification jobs and are not performance benchmarks.
The existing no-AA foliage stippling and bounded vegetation capture LODs remain
visible approximation limits.

Artifacts are under **`/tmp/ai-loop-worker-reports/J20261030-180654-207139/gi09/`**:
`countryside/` contains the 20 PNG/HDR pairs; `countryside-<view>.log` contains
inputs, native draw counts and validation; `regions.txt`, `analyze.cpp` and
`measure-regions.sh` retain independent C++ measurements and gates;
`pngs.sha256` / `hdr.sha256` identify every inspected snapshot. The same
`countryside.json` and `capture-views.sh` reproduce them through the crash-safe
wrapper. For each of `ground forest-edge details water overview`, from VTL:

```sh
bin/debug/exe/countryside --tessellated --config "$report/countryside.json" \
  --seed 42 --weather clear --time 12 --renderer hybrid-gi --gi on --ao off \
  --gi-view "$view" --capture-size 640x360 --frames 66 --gi-capture "$report/countryside"
```

GI-09 final regression: VVE Debug configure/build presets succeeded; both
`./build_linux.sh debug` and `./build_linux.sh release` passed **217/217** tests.
Debug `ctest -N` lists **225** registrations, with **62 nonempty GI-labelled**
tests. Final Debug `ctest -L gi --output-on-failure` passed **62/62** and
`ctest -L deferred --output-on-failure` **31/31**. Complete Release
`ctest --test-dir build/release-linux --output-on-failure` passed **225/225**,
including all eight Racer runs. Both 1x/MSAA4 HybridGiEquivalence mean, surface,
maximum RGB and zero-strength differences remain **exactly zero**.

In VTL, `./build_linux.sh debug --quick` and `./build_linux.sh release --quick`
each passed **107/107**, including **all eight registered GI cases**. The required
Release subset passed **7/7**:

```sh
ctest --test-dir build/release-linux -R 'ExampleCameraControls|TerrainTessellationGpu|TerrainLandscapeGpu|TerrainDetailsContracts|TerrainTreesGpu|test_terrain_water' --output-on-failure
```

Debug messenger errors and checked external
Release synchronization-validation diagnostics were **zero**; no required GPU
case was skipped. The 25 `slow` VTL tests were outside these quick runs.

Seven Release VVE native tests initially required a Debug-only messenger. Their
repaired harness now requires the real external Khronos synchronization layer
and rejects its error diagnostics; running without that configuration fails
with exit 2. No lighting threshold changed. VTL's earlier 60-second landscape
timeout is retained alongside its prior baseline occurrence; the isolated final
Debug test passed in **50.27 s** under the same limit (Release subset **42.86 s**).
The initial full Debug rebuild also exhausted the outer 1800-second batch
wrapper; its successful rerun used 3600 seconds, with individual CTest limits
unchanged. Final build/test logs, `results.json` and the test-to-acceptance map
are in the GI-09 artifact directory. No renderer regression remains known.

# Passes and shared state

The explicit dynamic-rendering sequence is:

1. Existing directional cascades, spot layers and point-light faces.
2. Hybrid only: cached probe-sun depth if stale, bounded probe faces and SH/visibility integration.
3. Opaque geometry and alpha-cutout materials into three G-buffer images and depth.
4. Hybrid only, when enabled: spatial contact AO evaluation and bilateral filtering.
5. A fullscreen triangle computes lighting into the linear RGBA16F HDR image.
6. The existing forward shaders draw sky and sorted transparent objects, sprites
   and translucent animated water using the preserved opaque depth.
7. Existing VVPPL post-processing (or the existing HDR blit).
8. ImGui composition and presentation.

Scene/asset catalogues, material tables and texture colour-space handling, GPU
uploads, per-slot dynamic vertex buffers, visibility/frustum culling, shadow
packing and rendering, cameras, windows, resource retirement, readback and
post-processing remain shared. Material sampling and lighting each have one
Slang implementation used by both paths. Backend-specific code owns only the
G-buffer, sampled-image descriptors, geometry/lighting pipelines, the GI probe
atlas/capture pipelines and their recording. There is no render graph or second scene implementation.

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

Hybrid GI reserves **eleven additional sampled-image bindings in total relative
to forward**, including the G-buffer and probe-shadow sampler. It additionally
requires sampled/color/transfer RGBA32F probe storage and sampled/color R16F AO
support. The same complete image-usage checks apply. No ray-tracing extension
or RTX hardware is required. Unsupported selections return `platform_error`
transactionally as described at startup; no backend is silently substituted.

G-buffer writes become shader-readable before lighting. Depth returns to its
attachment layout for forward composition; HDR writes are synchronized before
attachment loads and final transfer. Attachments shared by a window's two
frame slots have explicit dependencies across submissions on the shared graphics
queue. Resizing waits through the existing idle boundary, releases descriptors
before views/images and rebuilds all window-sized attachments. Minimized windows
retain their resources and resume through the existing swapchain recovery path.
Scene replacement uses the ownership and retirement contract above.

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

Earlier forward/deferred-only verification on Linux used Clang/libc++ 18, Vulkan SDK 1.4.350.1 and
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
