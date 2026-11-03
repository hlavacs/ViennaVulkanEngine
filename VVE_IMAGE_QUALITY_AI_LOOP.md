# AI-loop task: anti-aliasing, tone mapping and photographs for the countryside

## Objective and authorization

Make the VTL countryside example look closer to EZ-Tree's scenes (github.com/dgreenheck/ez-tree, checked out next to VTL as `ez-tree`). A comparison on 2 November 2026 found five gaps. This task closes three of them, as requested by the maintainer:

1. **Anti-aliasing.** EZ-Tree uses 4x MSAA plus SMAA 1x. VTL renders without anti-aliasing.
2. **Tone mapping.** EZ-Tree uses three.js `NeutralToneMapping` (Khronos PBR Neutral) at exposure 2. VTL uses vvppl's ACES at exposure 0.85. The observed brightness and colour difference also depends on lighting and materials; isolate these factors before assigning a cause.
3. **Photographs.** EZ-Tree uses photographic leaves, bark and ground. VTL's leaves and bark are procedural; its ground pipeline already includes a tinted Poly Haven grass scan.

The maintainer decided the following on 2 November 2026:

- **DEC-IQ-1:** MSAA and SMAA are options in VVE's simple renderer, and both are off by default. SMAA is implemented inside VVE, independent of vvppl. vvppl stays pinned and unchanged; VVE DEC-8 still holds.
- **DEC-IQ-2:** Khronos PBR Neutral tone mapping is an opt-in VVE option. VVE's default render path and appearance remain unchanged, verified against pre-task captures using the comparison policy below. Do not promise identical GPU pixels across runs, drivers or platforms.
- **DEC-IQ-3:** the countryside example uses the photographs by default (leaves, bark and ground); `--textures procedural` selects the historical texture set, including the existing tinted grass scan where image support is enabled. It selects materials only, not lighting or tone mapping. This applies only to the examples. The library in `src/` keeps its procedural materials as default and fallback, so VTL DEC-15 still holds for the library.
- **DEC-IQ-4:** the countryside example's tessellated default view uses the new Neutral tone mapping, calibrated with EZ-Tree as an artistic reference (see IQ-09), not a statistical ground truth for another scene. Anti-aliasing stays off by default; `--antialiasing <mode>` selects it. A separate legacy lighting selection preserves the full pre-task appearance configuration.

Record DEC-IQ-1 and DEC-IQ-2 as DEC-9 in VVE's `AI_LOOP_TASKS.md` decision table. Record DEC-IQ-3 and DEC-IQ-4 as DEC-23 and DEC-24 in VTL's `AI_LOOP_TASKS.md` decision table.

The requested result is working code, Linux builds, automated tests, inspected captures and documentation in both repositories. Do not stop after adding enum values or a design note.

Use this file as the task brief supplied to AI-loop. Start in the Linux VVE checkout. Expect sibling checkouts:

```text
<workspace>/ViennaVulkanEngine/
<workspace>/ViennaTerrainLibrary/
<workspace>/ez-tree/          (optional; only for comparison renders)
```

Resolve the actual paths locally. Do not use Windows paths.

## Read the current checkout before implementing

Read `AGENTS.md` and `CLAUDE.md` in both repositories, `src/versions/simple/AGENTS.md`, and the current `AI_LOOP_TASKS.md` rules ("How to work") of both repositories. Then inspect these sources.

VVE:

- `src/Types.ixx`: `Antialiasing`, `NativeDrawContext`, `RendererBackend`.
- `src/RenderSystem.ixx`: `setAntialiasing`, `setPostProcessSetup`, `captureFrameLinear`, `captureFrameToPng`.
- `src/versions/simple/Render/Renderer.ixx`: `WindowTarget` (`hdrImage`, `msaaImage`, `postProcess`), `sampleCount()`.
- `src/versions/simple/Render/RendererDraw.cpp`: in `recordCommandBuffer`, look at the MSAA resolve into `hdrImage`, then `postProcess->apply(...)` or the nearest `vkCmdBlitImage` to the swapchain, then the GUI pass on `ownedGuiViews`.
- `src/versions/simple/Render/RendererResources.cpp`: `setAntialiasing`, the MSAA capability checks and per-target image creation.
- `src/versions/simple/Render/RendererDeferred.cpp`: the per-sample G-buffer and lighting shaders.
- `src/versions/simple/Render/RendererDebug.cpp`: `captureFrameLinear` reads `hdrImage`; `captureFrameToPng` reads the swapchain and has a UNORM-swapchain branch.
- Tests: `tests/RenderAntialiasingTests.cpp`, `tests/RenderBackgroundColorTests.cpp`, `tests/support/TestSupport.ixx` and `tests/CMakeLists.txt`.

VTL:

- `examples/countryside/Tessellated.cpp`, the default path.
  - Its `setPostProcessSetup` line installs vvppl ACES at exposure 0.85.
  - It also sets the sun (`vve::DirectionalLight`), calls `setMaterialLighting(physically_based)` and calls `previewWeather(...)`.
- `examples/countryside/{countryside.cpp,Options.cpp,Countryside.hpp}`: the options and argument tests (`check_arguments.cmake`).
- `examples/common/terrain_materials.cpp` and the `vtl_terrain_materials` rule in `examples/CMakeLists.txt`.
  - It packs four albedo+height layers (meadow, fresh soil, limestone, snow) and four normal+roughness layers into `terrain-materials.bin`, at 512², 10 mips.
  - Layer 0 currently uses VVE's Poly Haven grass scan, re-tinted to the recipe colour.
- `examples/common/TreeAssets.hpp`, `TreeBarkMaps.hpp`, `shaders/trees_shared.h` and `shaders/trees.slang`:
  - eight species;
  - 128 procedural leaf-spray cards per species, as 6-bit coverage masks plus a 4-bit pigment;
  - procedural bark at 256² with 9 mips;
  - the CPU orthographic raster that must match the live leaf mask exactly;
  - impostors.
- `examples/common/shaders/terrain.slang` (`stochastic()`, `projection()`) and `terrain_surface.slang`.
- `cmake/VTLVVENativeDraw.cmake`: it checks the VVE checkout for required API symbols. Extend it for the new API.
- `examples/countryside/models/ez-tree/`: the GLB trees and how they are copied next to the executable (`examples/countryside/CMakeLists.txt`).
- **New, already in the VTL working tree (added 2 November 2026):** `examples/common/textures/ez-tree/`, the imported photographs. See its `README.md` for the files, sources, licences, SHA-256 sums, measured mean colours and the proposed species assignment.

### Starting facts, to recheck on Linux

1. `Antialiasing{none, msaa4}` exists. msaa4 is implemented for forward, deferred (per-sample shading) and, by construction, hybrid_gi. `NativeDrawContext::samples` reports the sample count. The existing test covers forward and deferred.
2. VVE's only tone mapper is vvppl's ACES (Narkowicz) compute effect. Without a vvppl setup, `hdrImage` is blitted unmodified to the swapchain.
3. VTL's countryside default capture (seed 12345, clear, 12:00 UTC, 960x540) has a bottom-quarter ("ground region") mean display luma of about 0.27-0.33 and mean saturation of 0.34-0.56. This was measured on Windows captures in `build/.../verify`, using sRGB-encoded PNG values, Rec. 709 luma coefficients and HSV saturation. These are display statistics, not linear-light luminance; the bottom quarter is not necessarily all ground.
4. The EZ-Tree reference image `src/app/public/og-image.png` has these bottom-quarter display statistics from a different scene/view (advisory only, not pass/fail targets):
   - mean display luma 0.471, median 0.482;
   - mean HSV saturation 0.385;
   - mean RGB 0.425, 0.499, 0.333.
5. The imported photos are dark in linear terms: the grass photo's linear mean is 0.068, 0.075, 0.034. EZ-Tree reaches its brightness through the following:
   - its sun intensity is 5 (three.js units, Lambert divided by pi);
   - ambient light is 0.4;
   - emissive ground is 0.01;
   - exposure is 2;
   - Neutral tone mapping.

   Do not brighten or recolour photographs to force another scene's statistics. Calibrate lighting and exposure on controlled fixtures and inspect the countryside. The documented summer-aspen conversion and bounded artistic leaf variation are explicit exceptions, not exposure corrections.
6. `ash.png`, `oak.png` and `pine.png` are green. `aspen.png` is autumn yellow. The proposed per-channel recolour factor for summer foliage is (0.224, 0.686, 0.797); the README has the derivation.

These are observations, not proof that the Linux checkout is identical. Search by symbols, not line numbers.

## Required architecture

### VVE output chain (IQ-02, IQ-03)

Today the frame goes scene -> `hdrImage` -> (vvppl chain | blit) -> swapchain -> GUI. With the new options it becomes:

```text
scene -> hdrImage (linear HDR; captureFrameLinear reads this, unchanged)
      -> [vvppl chain, if a setup is set; destination = postImage instead of the swapchain when a VVE pass follows]
      -> [tone-map pass, if ToneMapping.op != none]            (full-screen triangle)
      -> [SMAA 1x: edges -> blend weights -> neighbourhood blending, if smaa or msaa4_smaa]
      -> swapchain
      -> GUI
      -> capture/readback when requested, then presentation
```

- A fresh renderer with neither option active creates no new image, pipeline, descriptor or pass. Disabling tone mapping returns to the existing path (unless startup SMAA remains selected); previously used resources retire after their fences. Preserve the existing default command-recording path, including its UNORM fallback. Verify pass/resource invariants and compare against pre-task captures; absence of a new pass alone does not prove image equivalence.
- vvppl documents its destination as an image that it writes with `vkCmdBlitImage`. VVE passes a per-window `postImage` (the HDR format and extent, usage TRANSFER_DST | SAMPLED) only when a VVE pass follows. Check that vvppl accepts a non-swapchain destination. Do not modify vvppl.
- The tone-map pass reads `postImage`, or `hdrImage` when there is no vvppl chain. It writes linear colour through the swapchain's sRGB attachment view, or the SMAA input when SMAA is on. Use the explicit UNORM policy below where no sRGB swapchain format is available.
- The SMAA input `ldrImage` is R8G8B8A8_SRGB with MUTABLE_FORMAT and a UNORM view:
  - Edge detection reads the UNORM view, because luma edges are detected on gamma-encoded values.
  - Neighbourhood blending reads the sRGB view, so blending happens in linear light.
  - Neighbourhood blending outputs linear colour through the swapchain's sRGB view, or follows the UNORM policy below. Edge/weight attachments are linear data formats, never sRGB.
  - When SMAA is on without tone mapping, an identity pass (clamp to 0..1) fills `ldrImage`.
- Define output encoding per recorded frame, not just by swapchain format:
  - With both new options off, retain the legacy blit and UNORM capture conversion unchanged.
  - With either new option active, an sRGB attachment performs the one linear-to-sRGB conversion. For R8G8B8A8_UNORM/B8G8R8A8_UNORM with SRGB_NONLINEAR colour space, encode sRGB once in the final shader. Do not require an unavailable sRGB swapchain view.
  - PNG readback must not encode an already encoded opt-in UNORM frame again. Keep the actual frame's encoding until readback completes, including across setting changes. Preserve channel order and alpha.
  - Compose GUI colours according to the attachment encoding without a second conversion of already encoded widget colours. Test with and without mutable swapchain views. The opt-in UNORM path may correct the existing fallback's dark screen appearance; default-path behaviour stays unchanged.
  - Check format features, attachment usages and mutable-view compatibility for new resources. Unsupported opt-in formats/colour spaces return a documented `platform_error` before submission, rather than silently using the wrong transfer function.
- Resources are per window, created on demand, rebuilt on resize and released by fenced retirement. Document ordering of intermediate writes across frames in flight; use per-slot resources where existing ordering is insufficient. Prepare `postImage` in GENERAL before vvppl writes it, then synchronize transfer writes to sampling. Synchronize each SMAA attachment write to its later read. Match acquire-semaphore waits to the actual first swapchain use on each path, including direct colour-attachment output. Use no new device-wide idle waits.
- Clear SMAA edge and blend-weight attachments each frame as required by the reference, including pixels skipped by discard. Test a patterned frame followed by a flat frame to catch stale data.
- Tone mapping setters run on the renderer calling thread between frame calls and affect the next recorded frame; no concurrent-setter/thread-safety promise is introduced. Test enable, exposure changes, disable and re-enable with submissions still in flight. Retire descriptors/resources only after their last-use fences. Anti-aliasing stays startup-only: extend the existing `already_initialized` contract to the new values.
- VVE does not detect a vvppl tonemap effect in the chain. Document that combining both tone-maps twice.

### Public API (proposals to implement)

```cpp
// src/Types.ixx
enum class Antialiasing { none, msaa4, smaa, msaa4_smaa };   // smaa = 1 sample + SMAA 1x
enum class ToneMappingOperator { none, khronos_neutral };
struct ToneMapping { ToneMappingOperator op{ToneMappingOperator::none}; Scalar exposure{1}; };
// src/RenderSystem.ixx
auto setToneMapping(ToneMapping) -> std::expected<void, Error>;   // invalid_argument: unknown op, exposure not finite or <= 0
auto toneMapping() const -> ToneMapping;
```

`sampleCount()` is 4 for msaa4 and msaa4_smaa. The facade stays implementation-free. Update `examples/FACADE_AUDIT.md`.

### Khronos PBR Neutral

Implement the reference operator from KhronosGroup/ToneMapping, `PBR_Neutral` (Apache-2.0). Cite it in the Doxygen comment. It is identical to three.js `NeutralToneMapping`, which multiplies by the exposure first:

```glsl
c *= exposure;
const float startCompression = 0.8 - 0.04;
const float desaturation = 0.15;
float x = min(c.r, min(c.g, c.b));
float offset = x < 0.08 ? x - 6.25 * x * x : 0.04;
c -= offset;
float peak = max(c.r, max(c.g, c.b));
if (peak < startCompression) return c;
const float d = 1.0 - startCompression;
float newPeak = 1.0 - d * d / (peak + d - startCompression);
c *= newPeak / peak;
float g = 1.0 - 1.0 / (desaturation * (peak - newPeak) + 1.0);
return mix(c, vec3(newPeak), g);
```

Keep a CPU twin in the test, not the engine. Pin the reference revision and add independently calculated fixed vectors at zero, compression boundaries and HDR values so a shared transcription error is detectable. Define and test handling of negative/non-finite scene inputs and finite-exposure overflow; retain the reference behaviour for ordinary non-negative inputs.

### SMAA 1x

- Use the reference implementation: Jimenez, Echevarria, Sousa and Gutierrez, "SMAA: Enhanced Subpixel Morphological Antialiasing", Eurographics 2012, from github.com/iryoku/smaa (MIT).
- Vendor `SMAA.hlsl`, `AreaTex.h`, `SearchTex.h` and the licence under VVE's usual third-party location, pinned to a commit recorded in a README there. Fetch them once while doing the task; the build must not download them.
- Compile through Slang, using the reference's HLSL path or `SMAA_CUSTOM_SL`.
- Use preset HIGH: threshold 0.1, 16 orthogonal and 8 diagonal search steps, corner rounding 25.
- Upload AreaTex (160x560 RG8) and SearchTex (64x16 R8) once, with the filtering the reference requires.
- Put the SMAA setup and recording into its own file (e.g. `RendererSmaa.cpp`) to keep file sizes in line with the repository's conventions.
- Add a third-party notice to README.

### VTL integration (IQ-04 to IQ-09)

- VVE integration/photo assets belong in `examples/` and `cmake/`; documentation and owning tests may also change. Do not change VTL public API, procedural defaults or introduce a VVE dependency in `src/`. A narrow engine-neutral correction in `src/input/Raster_Decode.cpp` is permitted only if a failing PNG/JPEG fixture proves it necessary. Add the owning decoder regression and run image-enabled/disabled library tests. Preserve unrelated channel contracts; do not force all PNGs to RGBA.
- `cmake/VTLVVENativeDraw.cmake` additionally requires `msaa4_smaa` in `src/Types.ixx` and `setToneMapping` in `src/RenderSystem.ixx`, with the same clear FATAL_ERROR text.
- New countryside options, each with argument tests in `check_arguments.cmake`, an entry in the report and an entry in the read-only ImGui panel:

  | Option | Values | Default |
  |---|---|---|
  | `--antialiasing` | `none`, `msaa`, `smaa`, `msaa+smaa` | `none` |
  | `--textures` | `photo`, `procedural` | `photo` |
  | `--tone-mapping` | `neutral`, `aces`, `none` | `neutral` on the tessellated path; the legacy mesh path keeps its current output |
  | `--lighting` | `calibrated`, `legacy` | `calibrated` on the tessellated path; `legacy` on the legacy mesh path |

  - `--antialiasing` applies to both the tessellated and the legacy path and is set before the first frame.
  - `--textures photo` falls back to procedural, with the report keys `textures.mode=procedural` and `textures.fallback=no_image_support`, when VTL is built without `VTL_WITH_IMAGES`.
  - Each option requires a value; bare flags, missing values and unknown values are argument errors.
  - `--tone-mapping aces` selects the existing ACES operator at exposure 0.85 only. `--lighting legacy` selects the pre-task sun, ambient, sky/fog and weather-lighting parameters through shared lighting code, not a duplicate weather system or renderer.
  - The full legacy appearance configuration is `--textures procedural --lighting legacy --tone-mapping aces --antialiasing none`, with the same path, seed, view and weather as its pre-task baseline. Neither procedural textures nor ACES alone restores all old inputs.
  - Report requested and effective selections. Reject/report unsupported explicit choices on the legacy mesh path instead of accepting and ignoring them.
- Photos are decoded and packed **at build time**, as `terrain_materials.cpp` already does. They become binary blobs copied next to the executable. Runtime does not decode JPEG or PNG.
  - Verify progressive JPEG and palette PNG with tRNS using known small fixtures and the imported leaf PNGs. Preserve leaf alpha as RGBA and existing channel semantics for other callers. Correct `Raster_Decode.cpp` only under the scoped exception above when a decoder defect is demonstrated.
  - Build-time inputs are listed as `DEPENDS`, so editing a photo rebuilds the blob.
- Ground (IQ-06):
  - A second packed file `terrain-materials-photo.bin` with the same layout.
  - Layer 0 (meadow) uses `grass.jpg`. Layer 1 (fresh soil) uses `dirt_color.jpg`, and its normal layer uses `dirt_normal.jpg` (OpenGL convention).
  - Keep the photos' own colour: no recipe tint, no colour stamps from `addGroundDetail`. Relief stamps are allowed.
  - Height and roughness channels come from the recipe.
  - Limestone and snow stay procedural.
  - EZ-Tree repeats both photos every 30 m (`ground.js`). Pick VTL's physical size per layer by inspection at the 1.8 m G-key height and at 30 m, and document it. Keep the existing stochastic triangle sampling, so no repetition becomes visible.
- Leaves (IQ-07):
  - In photo mode, each species' leaf cards sample the assigned photo spray (colour and alpha), following the assignment in the photo README.
  - Variation comes from mirroring, rotation, a sub-rectangle choice and a small per-instance hue/value jitter, instead of the 128 procedural shapes.
  - Alpha drives coverage with the existing stable per-sample threshold (`treeCoverageThreshold`). Mips preserve coverage (alpha-to-coverage-preserving mip generation), so distant crowns do not thin out.
  - Shadow, G-buffer, probe and impostor passes use the same alpha/coverage definition. The CPU raster uses matching source data, UV transforms, filtering and thresholds. Require exact agreement on shared discrete mask samples and bounded coverage differences at raster boundaries, not bit-identical continuous CPU/GPU rasterization.
  - `aspen.png` is used for birch and baobab with the summer recolour factor.
  - Needles (spruce, pine) keep their higher mask resolution.
  - Respect the existing buffer budgets, or document an increase and the reason.
- Bark (IQ-08):
  - Photo bark replaces the procedural bark in the bark section of the atlas: 256² colour plus normal and roughness, 9 mips, filtered in linear space, normals renormalised.
  - It is used for live branches, the far bark ranges and impostors.
  - Silver birch keeps its procedural white bark, because no scan matches it.
  - The bark repeat in metres follows the scan's physical size (ambientCG 1K sets cover roughly 1-2 m; check the source pages listed in `TEXTURES-LICENSE.md`).
- Tone mapping (IQ-09): the tessellated path calls `setToneMapping({khronos_neutral, exposure})` instead of the vvppl ACES setup. See the calibration acceptance test below.

## AI-loop execution phases

Use focused commits following the repositories' established workflow (one task per commit, message `<ID>: <title>`). Preserve unrelated user changes. Record progress in each repository's loop log, without rewriting unrelated historical tasks.

| ID | Repo | Work | Completion gate |
| --- | --- | --- | --- |
| IQ-00 | both | Audit the current renderer, options and asset pipeline; record DEC-9 / DEC-23 / DEC-24; freeze pre-task captures, full legacy inputs, comparison limits and controlled quality fixtures. | Immutable baseline manifest, repeat measurements, masks and captures saved under `build/<dir>/verify/image-quality/baseline` and archived outside disposable build outputs; existing failures distinguished from new ones. |
| IQ-01 | VVE | Verify msaa4 with forward, deferred and hybrid_gi, each with and without a native-draw callback that builds its pipelines from `context.samples`, `colorAttachmentCount` and `colorFormats`. Fix what fails. | Acceptance test 1. README states the supported backends and the native-draw rule. |
| IQ-02 | VVE | Output chain plus opt-in Khronos Neutral tone mapping. | Acceptance tests 2 and 3. |
| IQ-03 | VVE | Opt-in SMAA 1x (`smaa`, `msaa4_smaa`). | Acceptance tests 4 and 5. |
| IQ-04 | VTL | AA/lighting options, retained legacy lighting inputs, API check, panel and report; photo/tone options follow their owning phases. | Acceptance test 6 for available options; complete option matrix in IQ-09 and legacy preservation in test 7. |
| IQ-05 | VTL | Photo asset pipeline: build-time packing, copying next to the executable, `--textures`, the fallback without images, licences in the install tree. | Acceptance test 7. |
| IQ-06 | VTL | Photo ground layers. | Acceptance test 8. |
| IQ-07 | VTL | Photo leaf sprays for the procedural trees. | Acceptance test 9. |
| IQ-08 | VTL | Photo bark. | Acceptance test 10. |
| IQ-09 | VTL | Neutral tone mapping and calibration of exposure and lights for every weather preset. | Acceptance test 11. |
| IQ-10 | both | Gallery, documentation and final evidence. | Acceptance test 12; all docs updated; final status summary. |

Do IQ-01 to IQ-03 in VVE first and commit them. VTL's API check then requires that VVE revision. IQ-09 comes after IQ-06 to IQ-08, because the photos change the brightness. Phases may be split into smaller commits. If a requirement is already implemented, verify and reuse it. When a test fails, diagnose the image, data or validation output instead of relaxing the test.

## Evidence and comparison policy

- Before rendering changes, capture each preservation fixture at least three times with the same executable, GPU, driver and settings. Archive immutable PNGs/decoded hashes plus source revisions, dirty-tree patch/hash, dependencies/toolchain, device/driver, format/colour space, dimensions, camera, seed, animation/weather time, GUI visibility, warm-up and capture frame. Keep a copy outside disposable build trees. Freeze animation and, for hybrid GI, cache reset, convergence/warm-up and frame sequence. Never replace references with candidate output.
- Compare decoded RGBA, with exact dimensions/alpha, not PNG container bytes. Record changed RGB count/fraction, maximum channel error, mean error and difference images. Freeze per-fixture limits from measured baseline variation before implementation. Use exact pixels for stable fixtures and narrowly bounded measured tolerances for unstable ones, with a deliberate defect that must fail. Do not transfer one fixture's tolerance to another, rely on a loose whole-image average or widen limits after candidate failures.
- Cross-driver/platform images are separate evidence, not strict pixel baselines. If repeat variability prevents a useful bound, record the blocker and use independent analytic/structural checks; do not claim preservation from self-generated candidate references. Missing/unreadable references fail; they never create a replacement or count as a pass.
- Deterministic generated asset blobs require byte equality and input/output hashes. Separate GPU preservation of defaults/full legacy appearance from intentional calibrated changes. Candidate default-vs-explicit-default comparisons are API checks, not substitutes for pre-task regression comparisons.
- Use independent object/material masks, excluding GUI, sky, water and unrelated objects. Record mask construction, coverage and minimum sample count; freeze them before tuning. Missing/too-small regions fail the fixture or remain unsupported, never pass vacuously. Distinguish linear RGB/luminance from sRGB display luma/HSV. Use alpha-weighted colour and circular hue means, excluding hue at a documented low saturation threshold.
- Freeze quality limits using known fixtures/independent references and negative controls (bypassed AA, blur, wrong transfer curve, missing alpha, wrong LOD). Cite existing tolerances by test/constant and numeric value; if none exists, define and justify one before changing rendering. Do not retune checks/references just to accept an implementation.

## Required acceptance tests

Register native C++ tests with CTest using the existing support modules and conventions. Use the label `image_quality` in both repositories, and check that the filtered run actually contains tests. Do not introduce Python tests. Use deterministic scenes, cameras, seeds and times.

1. **MSAA coverage (VVE):** extend `RenderAntialiasingTests` for forward, deferred and hybrid_gi, with/without a native tilted quad. Use known unlit geometry at several subpixel offsets and an independent analytic-coverage or at least 8x-per-axis supersampled reference, averaged in linear light at the same resolve stage before the output transform. Require reduced aggregate edge error versus none and preserved interior colour/feature coverage using frozen fixture limits. Edge-pixel counts are diagnostics only. Do not require MSAA to smooth fragment-shader stripes without sample-frequency coverage changes. Unsupported 4x devices must return the existing `platform_error`, preserve state and recover with none; this tests capability handling, not 4x quality. Zero validation errors.
2. **Tone-mapping exactness (VVE):** `RenderToneMappingTests` uses empty linear backgrounds (0.02, 0.18, 1.0), (4, 2, 0.5), zero and independently calculated compression-boundary vectors, with op none/Neutral and exposures 1/2.
   - Scene-only pixels match the independent CPU/fixed-vector reference within 1/255 after the documented transfer curve and format quantization; hide GUI. Model legacy UNORM linear 8-bit quantization before capture encoding. Separately check opt-in UNORM/sRGB paths, no mutable swapchain views, channel order, alpha and exactly one encoding.
   - op none preserves the old background contract; `captureFrameLinear` returns untouched HDR within a documented HDR-format tolerance, independent of display processing.
   - Cover all backends, msaa4, resize and two windows, with/without a known analytic vvppl colour-gain chain and a separate documented ACES-then-Neutral composition case. Never use GPU output as its own reference.
   - Invalid operators/exposures (zero, negative, NaN, infinity) preserve stored state and output. Also test the documented negative/non-finite scene-input and overflow policy.
3. **Default unchanged (VVE):** compare lighting/Sponza captures against immutable pre-task baselines under the comparison policy, including the legacy UNORM fallback. Separately compare default with explicit `ToneMapping{}`/`Antialiasing::none` as an API check. Verify no tone-map/SMAA recorded pass and no new post images/pipelines/descriptors in a fresh default renderer. Previously enabled renderers may retain only resources awaiting fenced retirement; do not confuse them with fresh-default allocations.
4. **SMAA effect (VVE):** test tilted edges at several offsets, thin lines, corners, diagonals and shader/alpha-cutout detail in all four modes, with pinned supersampled references and fixed masks.
   - Measure edge error and thin-feature coverage/contrast. Require SMAA to reduce aggregate error versus none on selected edge fixtures. Require msaa4_smaa to improve a preselected residual-aliasing fixture and avoid material aggregate regression versus msaa4 within a justified frozen limit. Do not assume monotonic improvement for every image, or recovery of missing subpixel detail by SMAA 1x.
   - Freeze numeric error/detail limits in IQ-00 before implementation. Bypassed SMAA must fail an effect check; a blur negative control must fail detail/contrast or coverage checks even if intermediate-pixel counts improve.
   - Outside an independently defined affected-edge mask, pixels change by at most 1/255. Derive the mask from fixture geometry and the pinned filter's support, never from candidate difference images.
   - A patterned frame followed by a flat frame has an all-zero edge texture and no stale blend weights/output; read intermediates through an opt-in test path.
5. **SMAA and tone-mapping lifecycle (VVE):** cover all backends, startup AA modes, vvppl present/absent, two windows, independent resize, minimise/resume and repeated init/shutdown.
   - With frames in flight, run tone mapping none -> Neutral exposure 1 -> exposure 2 -> none -> re-enable, with/without startup SMAA. Verify the next recorded frame's colour/pass order/encoding and invalid setters between valid changes. Old-frame captures retain their recorded encoding. Queue ordinary frames across transitions before waiting for results; do not let a capture helper's implicit idle wait serialize every transition and hide lifetime/synchronization defects. Use fenced per-frame diagnostic readback where needed.
   - Resize one window during transitions; the other remains valid. Check retirement after last-use fences and bounded resource counts after repeated toggles; introduce no device-wide idle waits.
   - Require zero errors with synchronization validation demonstrably active in Debug, not merely a zero counter. Late AA changes return `already_initialized`; unknown values return `invalid_argument` under existing error precedence.
   - Test sRGB/UNORM output and GUI composition. Where the surface supports both, use test-only format selection. Otherwise run an offscreen format oracle and report the unavailable real-swapchain case without claiming coverage.
6. **VTL options and anti-aliasing:** argument tests cover spellings, missing values and unsupported/incompatible choices. Reports/panel show requested/effective AA, textures, lighting and tone mapping. Exercise tessellated and mesh paths as documented. Use controlled silhouette/terrain-edge views and the error/detail checks above; countryside intermediate-pixel counts are diagnostics, not monotonic pass criteria. Zero validation errors.
7. **VTL photo pipeline and legacy appearance:**
   - Generate `terrain-materials-photo.bin` and the tree photo blob with verified sizes/layouts and deterministic hashes. Editing a photo rebuilds affected outputs; unchanged inputs do not regenerate unrelated assets.
   - Install blobs and unambiguously named licence/attribution files beside/in the documented executable assets. Verify an executable copied with these assets runs without the source/build tree.
   - Compare `--textures procedural --lighting legacy --tone-mapping aces --antialiasing none` against pre-task captures with matching path, seed, camera, weather and frozen frame sequence. Preserve historical lighting/sky/fog values and texture-blob hashes, including the old tinted grass scan.
   - Procedural textures alone change materials; ACES alone changes tone mapping. Neither restores all old inputs. New photo+ACES captures may differ from the old default.
   - Image-disabled builds generate usable procedural blobs and report explicit fallback. Missing/malformed scans in image-enabled builds fail rather than silently fall back. If a decoder fix is necessary, known transparency/JPEG fixtures fail before it and pass afterward; unrelated channel contracts and image-enabled/disabled library tests stay intact.
8. **VTL ground:** verify packed photo albedo against independently decoded/resampled source texels in linear RGB with fixed filtering/quantization limits and a controlled unlit display fixture. Verify no recipe tint/colour stamps, and test normal orientation/channel packing separately. At 1.8 m and 30 m, use projected meadow-only masks for detail/repetition. Do not require lit hue to match a photograph or variance to exceed procedural textures. Quantify autocorrelation/FFT peaks with explicit windows/scales/limits and repeated-tile/nonrepeating controls. Repeated tiles and flat/noise substitutes must fail repetition or source-fidelity checks as appropriate.
9. **VTL leaves:**
   - Per species, compare shared discrete CPU/GPU alpha samples exactly and orthographic raster coverage with explicit boundary tolerances, matching view/UV/filtering. Verify source albedo in an unlit fixture; test summer-aspen against its specified linear transform, not autumn hue.
   - Coverage means occupied alpha samples divided by an independently projected fixed crown region at the same view direction. At 200 m and 600 m, compare actual LOD with forced LOD0 at the identical camera/projected footprint. Record numerator, denominator, projected area, sample count and LOD; exclude ground/shadows/GUI. Do not compare raw pixel counts between distances.
   - Apply the 10% relative coverage-error limit only to resolved crowns (at least 32x32 projected pixels and nonzero reference coverage). For smaller footprints, report actual-distance diagnostics and separately test equivalent LOD/mip coverage with a higher-resolution or subpixel-sampled fixture. Empty/tiny masks never pass vacuously. Add an isolated mip-coverage fixture with fixed alpha thresholds/sample patterns and an independent base-level reference, separate from geometry changes.
   - Shadow, G-buffer, probe and impostor tests retain known leaf holes. Missing-alpha/thinned-mip controls fail; record numeric tolerances before implementation.
10. **VTL bark:** verify packed albedo, normal orientation/renormalization, roughness, physical UV repeat and mip filtering against independent source texels/numeric fixtures. At 2 m, compare retained scan structure against the filtered source under controlled illumination, not against procedural variance. Birch retains pre-task procedural albedo; do not demand brightness above 0.6 from shaded bark. Test mesh/impostor hand-off with matching camera, projected size, illumination and material masks, an explicit numeric brightness-error limit and a mismatched-bark negative control.
11. **VTL calibration:** default photo mode, tessellated, seed 12345, clear, 12:00 UTC, 960x540, recorded camera, Neutral tone mapping and calibrated lighting.
    - Validate neutral swatches/grey ramp under fixed known lighting for albedo, exposure, transfer curve and highlight compression before artistic tuning. Do not change geometry, albedo/hue or add noise to meet image statistics.
    - Exposure 2 is a starting point, not a reason to transplant three.js light units. Record chosen exposure, sun, ambient/sky and weather settings/reasons. Preserve legacy lighting independently.
    - Report linear luminance and sRGB display luma/HSV separately for independent meadow, shaded-meadow, sky and snow masks where present. The former EZ-Tree bottom-quarter target ranges (display luma 0.44-0.50, saturation 0.33-0.42) are advisory only, never CTest gates. Different scene distributions are not equivalent; do not force overexposure to match them.
    - On sufficiently populated non-snow ground masks, require fewer than 2% of pixels with all RGB channels >=254 and fewer than 2% with all channels <=1 in encoded PNGs. Also report per-channel clipping. Define separate snow/highlight and deliberate night/shadow fixture limits before tuning; do not remove failing regions or force non-snow bounds onto white snow.
    - For all weather presets at 07:00, 12:00 and 18:00, record solar elevation and inspect same-camera pairs. Controlled same-hour/region fog/overcast fixtures must reduce contrast versus clear; require brightness ordering only where the defined illumination supports it. Measure snow whiteness in a snow-material mask. Freeze applicable numeric limits and negative controls before calibration.
    - Only identified old-ACES visual thresholds may change for the new calibrated profile; list old/new values and independent justification. Legacy-profile references/thresholds, validation and structural checks remain unchanged. Never update baselines/tolerances to hide unrelated regressions. Full legacy appearance still passes test 7.
12. **Side-by-side evidence:** CTests labelled both `gallery` and `image_quality` write paired captures, manifests, input/source hashes, masks, difference images and decoded metrics to `build/<dir>/verify/image-quality/`:
    - photo vs procedural with lighting/tone mapping fixed;
    - neutral vs aces with materials/lighting fixed;
    - none vs msaa+smaa with everything else fixed;
    - pre-task baseline vs full legacy appearance, separate from new-default comparisons;
    - default view, G-key ground view, forest edge and overview, plus a short fixed subpixel camera-motion sequence for shimmer/LOD transitions. SMAA 1x is spatial AA; report remaining temporal aliasing without promising temporal reconstruction.

    If `../ez-tree/src/app/public/og-image.png` exists, record its revision/hash and advisory bottom-quarter display statistics. Its absence is not a failure; missing required preservation/fixture references are failures. Each pair changes one factor and records the rest. Favourable statistics do not excuse visible blur, lost foliage or overexposure.

Open and look at every capture yourself before committing, and describe what it shows in the commit message, as VTL's Phase 8 rules require.

## Linux build and verification

Use the checked-in build scripts and the checkout's required toolchain. VVE, from its root:

```bash
./build_linux.sh debug
./build_linux.sh release
ctest --test-dir build/debug-linux -L image_quality --output-on-failure
ctest --test-dir build/release-linux --output-on-failure
```

VTL, after VVE contains IQ-01 to IQ-03:

```bash
cd ../ViennaTerrainLibrary
./build_linux.sh debug
./build_linux.sh release
ctest --test-dir build/release-linux -L image_quality --output-on-failure
ctest --test-dir build/release-linux --output-on-failure
```

Discover the actual test registrations. `--quick` builds are fine while iterating, but do not replace the final full runs. Enable the Khronos validation layer, including synchronization validation, for Debug runs according to the repository instructions. A skipped graphics test or a missing display is not a successful validation.

Record performance on a physical GPU at 1920x1080 with the fixed default view: separate GPU timestamps for tone mapping, all SMAA passes together and the full frame; report CPU frame time separately. Record device/driver, backend, present mode/VSync, timestamp support, warm-up and sample count (at least 100 warm-up and 300 measured steady-state frames, excluding loading/resize/readback). Compare all four AA modes with/without tone mapping under comparable scene/clock conditions. Report median and 95th percentile. The provisional 0.3 ms tone-map and 1.0 ms SMAA budgets apply to the named test GPU, not every device. Explain deviations. Missing hardware/timestamps leave that evidence pending; do not substitute CPU/software-renderer timings or claim completion. Do not invent Windows/macOS results; the maintainer checks Windows.

## Final deliverables and definition of done

- VVE: msaa4 verified on all backends and native draws; opt-in SMAA 1x and Khronos PBR Neutral tone mapping with defaults unchanged; tests; third-party notice; README, AGENTS, `src/versions/simple/AGENTS.md`, `docs/notes/architecture.md` and `examples/FACADE_AUDIT.md` updated.
- VTL:
  - `--antialiasing`, `--textures`, `--tone-mapping` and `--lighting`, with requested/effective selections reported;
  - photographs packed at build time and used by default for ground, leaves and bark;
  - full legacy appearance preserves pre-task captures within frozen per-fixture limits, with historical material blobs and lighting inputs unchanged;
  - Neutral tone mapping/calibrated lighting validated on controlled fixtures, using EZ-Tree as an inspected artistic reference;
  - tests and gallery;
  - `AGENTS.md`, README and the photo README updated with the final species assignment and physical scales.
- Successful Linux build/test logs, immutable baseline archive/manifests, inspected captures/difference images and measured timings. Final status lists changes, commands, executed/passed/failed/skipped counts, artifacts and blockers, distinguishing actual backend/hardware/format coverage from unsupported/unexecuted cases. Pending required evidence means that part is incomplete even if available tests pass.
- Do not push or publish unless the maintainer's workflow separately authorizes it.
