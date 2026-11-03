# AI-loop task: anti-aliasing, tone mapping and photographs for the countryside

## Objective and authorization

Make the VTL countryside example look closer to EZ-Tree's scenes (github.com/dgreenheck/ez-tree, checked out next to VTL as `ez-tree`). A comparison on 2 November 2026 found five gaps. This task closes three of them, as requested by the maintainer:

1. **Anti-aliasing.** EZ-Tree uses 4x MSAA plus SMAA 1x. VTL renders without anti-aliasing.
2. **Tone mapping.** EZ-Tree uses three.js `NeutralToneMapping` (Khronos PBR Neutral) at exposure 2. VTL uses vvppl's ACES at exposure 0.85, so its image is darker and greyer.
3. **Photographs.** EZ-Tree's leaves, bark and ground are photographs. VTL's are procedural.

The maintainer decided the following on 2 November 2026:

- **DEC-IQ-1:** MSAA and SMAA are options in VVE's simple renderer, and both are off by default. SMAA is implemented inside VVE, independent of vvppl. vvppl stays pinned and unchanged; VVE DEC-8 still holds.
- **DEC-IQ-2:** Khronos PBR Neutral tone mapping is an opt-in VVE option. With VVE's defaults, every existing image stays bit-identical.
- **DEC-IQ-3:** the countryside example uses the photographs by default (leaves, bark and ground); `--textures procedural` restores the procedural look. This applies only to the examples. The library in `src/` keeps its procedural materials as default and fallback, so VTL DEC-15 still holds for the library.
- **DEC-IQ-4:** the countryside example's default view uses the new Neutral tone mapping, calibrated to the brightness and saturation of EZ-Tree's reference image (see IQ-09). Anti-aliasing stays off by default; `--antialiasing` turns it on.

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
3. VTL's countryside default capture (seed 12345, clear, 12:00 UTC, 960x540) has a bottom-quarter ("ground region") mean luminance of about 0.27-0.33 and mean saturation of 0.34-0.56. This was measured on Windows captures in `build/.../verify`, using sRGB-encoded PNG values, Rec. 709 luma and HSV saturation.
4. The EZ-Tree reference image `src/app/public/og-image.png` has a ground region with these values:
   - mean luminance 0.471, median 0.482;
   - mean HSV saturation 0.385;
   - mean RGB 0.425, 0.499, 0.333.
5. The imported photos are dark in linear terms: the grass photo's linear mean is 0.068, 0.075, 0.034. EZ-Tree reaches its brightness through the following:
   - its sun intensity is 5 (three.js units, Lambert divided by pi);
   - ambient light is 0.4;
   - emissive ground is 0.01;
   - exposure is 2;
   - Neutral tone mapping.

   Do not brighten or recolour the photographs to reach the target. Calibrate the lighting and exposure instead.
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
      -> swapchain (captureFrameToPng reads this)
      -> GUI
```

- If neither option is active, VVE creates no new image, pipeline, descriptor or pass. The command stream is exactly today's, so outputs are bit-identical.
- vvppl documents its destination as an image that it writes with `vkCmdBlitImage`. VVE passes a per-window `postImage` (the HDR format and extent, usage TRANSFER_DST | SAMPLED) only when a VVE pass follows. Check that vvppl accepts a non-swapchain destination. Do not modify vvppl.
- The tone-map pass reads `postImage`, or `hdrImage` when there is no vvppl chain. It writes the swapchain's sRGB view, or the SMAA input when SMAA is on.
- The SMAA input `ldrImage` is R8G8B8A8_SRGB with MUTABLE_FORMAT and a UNORM view:
  - Edge detection reads the UNORM view, because luma edges are detected on gamma-encoded values.
  - Neighbourhood blending reads the sRGB view, so blending happens in linear light.
  - Neighbourhood blending writes the swapchain's sRGB view.
  - When SMAA is on without tone mapping, an identity pass (clamp to 0..1) fills `ldrImage`.
- UNORM swapchains must give the same PNG and screen result rule as today's blit path. Check the UNORM branch in `captureFrameToPng` and the GUI's `ownedGuiViews`.
- Resources are per window, created on demand, rebuilt on resize, and released through the existing retirement rules. Barriers and layouts must be correct; use no new device-wide idle waits.
- Tone mapping can change at any time and takes effect on the next frame. Anti-aliasing stays startup-only: the existing `already_initialized` contract is extended to the new values.
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

Keep a CPU twin of the formula in the test, not in the engine.

### SMAA 1x

- Use the reference implementation: Jimenez, Echevarria, Sousa and Gutierrez, "SMAA: Enhanced Subpixel Morphological Antialiasing", Eurographics 2012, from github.com/iryoku/smaa (MIT).
- Vendor `SMAA.hlsl`, `AreaTex.h`, `SearchTex.h` and the licence under VVE's usual third-party location, pinned to a commit recorded in a README there. Fetch them once while doing the task; the build must not download them.
- Compile through Slang, using the reference's HLSL path or `SMAA_CUSTOM_SL`.
- Use preset HIGH: threshold 0.1, 16 orthogonal and 8 diagonal search steps, corner rounding 25.
- Upload AreaTex (160x560 RG8) and SearchTex (64x16 R8) once, with the filtering the reference requires.
- Put the SMAA setup and recording into its own file (e.g. `RendererSmaa.cpp`) to keep file sizes in line with the repository's conventions.
- Add a third-party notice to README.

### VTL integration (IQ-04 to IQ-09)

- Only `examples/` and `cmake/` change. `src/` does not change, because VTL stays engine-neutral and the photos are an example asset.
- `cmake/VTLVVENativeDraw.cmake` additionally requires `msaa4_smaa` in `src/Types.ixx` and `setToneMapping` in `src/RenderSystem.ixx`, with the same clear FATAL_ERROR text.
- New countryside options, each with argument tests in `check_arguments.cmake`, an entry in the report and an entry in the read-only ImGui panel:

  | Option | Values | Default |
  |---|---|---|
  | `--antialiasing` | `none`, `msaa`, `smaa`, `msaa+smaa` | `none` |
  | `--textures` | `photo`, `procedural` | `photo` |
  | `--tone-mapping` | `neutral`, `aces`, `none` | `neutral` on the tessellated path; the legacy mesh path keeps its current output |

  - `--antialiasing` applies to both the tessellated and the legacy path and is set before the first frame.
  - `--textures photo` falls back to procedural, with the report keys `textures.mode=procedural` and `textures.fallback=no_image_support`, when VTL is built without `VTL_WITH_IMAGES`.
  - `--tone-mapping aces` reproduces today's vvppl ACES at 0.85 exactly, for comparisons.
- Photos are decoded and packed **at build time**, as `terrain_materials.cpp` already does. They become binary blobs copied next to the executable. Runtime does not decode JPEG or PNG.
  - stb_image (in `Raster_Decode.cpp`) reads progressive JPEG and palette PNG with tRNS. Verify that `readPng8` returns four channels for the leaf PNGs, and fix it if not.
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
  - Shadow, G-buffer and probe-capture passes and the impostor bake use the same mask, and so does the CPU raster in `TreeAssets.hpp`, which must match the live shader exactly as it does today.
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
| IQ-00 | both | Audit the current renderer, options and asset pipeline; record DEC-9 / DEC-23 / DEC-24; record baseline captures and their ground-region statistics with the current defaults. | Baseline captures and numbers saved under `build/<dir>/verify/image-quality/baseline`; existing failures distinguished from new ones. |
| IQ-01 | VVE | Verify msaa4 with forward, deferred and hybrid_gi, each with and without a native-draw callback that builds its pipelines from `context.samples`, `colorAttachmentCount` and `colorFormats`. Fix what fails. | Acceptance test 1. README states the supported backends and the native-draw rule. |
| IQ-02 | VVE | Output chain plus opt-in Khronos Neutral tone mapping. | Acceptance tests 2 and 3. |
| IQ-03 | VVE | Opt-in SMAA 1x (`smaa`, `msaa4_smaa`). | Acceptance tests 4 and 5. |
| IQ-04 | VTL | `--antialiasing`, the API check in `VTLVVENativeDraw.cmake`, the panel and the report. | Acceptance test 6. |
| IQ-05 | VTL | Photo asset pipeline: build-time packing, copying next to the executable, `--textures`, the fallback without images, licences in the install tree. | Acceptance test 7. |
| IQ-06 | VTL | Photo ground layers. | Acceptance test 8. |
| IQ-07 | VTL | Photo leaf sprays for the procedural trees. | Acceptance test 9. |
| IQ-08 | VTL | Photo bark. | Acceptance test 10. |
| IQ-09 | VTL | Neutral tone mapping and calibration of exposure and lights for every weather preset. | Acceptance test 11. |
| IQ-10 | both | Gallery, documentation and final evidence. | Acceptance test 12; all docs updated; final status summary. |

Do IQ-01 to IQ-03 in VVE first and commit them. VTL's API check then requires that VVE revision. IQ-09 comes after IQ-06 to IQ-08, because the photos change the brightness. Phases may be split into smaller commits. If a requirement is already implemented, verify and reuse it. When a test fails, diagnose the image, data or validation output instead of relaxing the test.

## Required acceptance tests

Register native C++ tests with CTest using the existing support modules and conventions. Use the label `image_quality` in both repositories, and check that the filtered run actually contains tests. Do not introduce Python tests. Use deterministic scenes, cameras, seeds and times.

1. **MSAA coverage (VVE):** the extended `RenderAntialiasingTests` prints edge-pixel counts for none and msaa4 for forward, deferred and hybrid_gi, each with and without a native tilted quad.
   - msaa4 has more intermediate edge pixels than none in every case.
   - Devices without 4x support take the existing `platform_error` path.
   - There are zero validation errors.
2. **Tone-mapping exactness (VVE):** a new `RenderToneMappingTests` clears empty scenes to linear background colours (0.02, 0.18, 1.0) and (4, 2, 0.5). It captures PNGs with op none and with khronos_neutral at exposure 1 and 2.
   - Every pixel matches the CPU reference, sRGB-encoded, within 1/255.
   - op none matches `RenderBackgroundColorTests`' expectation.
   - `captureFrameLinear` returns the untouched HDR values in all cases.
   - It works with forward, deferred and hybrid_gi, with and without a vvppl chain (with a vvppl chain, compare against vvppl-then-Neutral), with msaa4, after a resize and with two windows.
   - `setToneMapping` rejects unknown operators and exposures of 0, negative values, NaN and infinity without changing the stored setting.
3. **Default unchanged (VVE):** with default settings, `lastRecordedPassOrder()` contains no tone-map or SMAA pass. No `postImage` or `ldrImage` exists. A representative existing capture test (lighting or Sponza) is byte-identical between the default and an explicit `setToneMapping(ToneMapping{})` with `Antialiasing::none`.
4. **SMAA effect (VVE):** render a high-contrast tilted edge and a one-pixel stripe pattern produced by a shader or alpha test, for none, smaa, msaa4 and msaa4_smaa.
   - smaa has more intermediate edge pixels than none; msaa4_smaa has more than msaa4.
   - Pixels farther than 4 px from any edge change by at most 1/255.
   - A flat-colour frame produces an all-zero edge texture; read it back in a debug-only path.
5. **SMAA robustness (VVE):** forward, deferred and hybrid_gi; with and without tone mapping and a vvppl chain; resize; two windows; minimise and resume; repeated init and shutdown.
   - Zero validation errors, including synchronization validation in Debug.
   - Late `setAntialiasing` returns `already_initialized`; unknown values return `invalid_argument`.
6. **VTL anti-aliasing:** argument tests accept and reject the right spellings. Each mode reaches VVE (report key `render.antialiasing`). Captures of the default view in all four modes show increasing edge smoothness on tree silhouettes and the horizon: count intermediate pixels in fixed masks. Zero validation errors.
7. **VTL photo pipeline:**
   - The build produces `terrain-materials-photo.bin` and the tree photo blob.
   - Editing a photo triggers a rebuild.
   - The executable directory contains the blobs and the licence files (`LICENSE`, `TEXTURES-LICENSE.md`, `README.md`).
   - `--textures procedural` captures are byte-identical to the current default captures (before this task) for the same seed, view, weather and tone mapping.
   - A build without images reports the fallback instead of failing.
8. **VTL ground:** in the ground region of the default capture at 1.8 m and 30 m camera heights:
   - The mean hue of meadow areas lies within 15 degrees of the grass photo's mean hue. Compute both in the same colour space and document the method.
   - The luminance standard deviation is at least that of the procedural mode.
   - An autocorrelation or FFT check finds no periodic peak at the layer repeat, following the existing anti-repetition checks.
9. **VTL leaves:**
   - The CPU raster and a GPU capture of an isolated tree per species agree on coverage within the existing tolerance.
   - Crown coverage of distant trees at 200 m and 600 m stays within 10 % of the near-field value. This checks the coverage-preserving mips.
   - The leaf colour mean of each species lies within 20 degrees of hue of its assigned spray; birch and baobab use the recoloured aspen.
   - Shadow and impostor captures keep the leaf holes.
10. **VTL bark:** close-up captures of trunks per species show the scan's structure. The luminance standard deviation at 2 m is above the procedural bark's. Birch remains white (mean luminance above 0.6 in its trunk mask). Impostor-to-mesh hand-off shows no brightness jump above the existing tolerance.
11. **VTL calibration:** default photo mode, tessellated, seed 12345, clear, 12:00 UTC, 960x540, default view, Neutral tone mapping. Ground region = the bottom quarter of the image, using sRGB PNG values, Rec. 709 luma and HSV saturation.
    - Mean luminance lies in 0.44-0.50.
    - Mean saturation lies in 0.33-0.42.
    - Fewer than 2 % of the ground pixels are clipped white and fewer than 2 % are clipped black.
    - Calibrate sun intensity, sky/ambient and exposure. Prefer exposure 2, as in EZ-Tree, with VTL's light intensities scaled to match, and record the chosen values and the reasons.
    - Every weather preset at 07:00, 12:00 and 18:00 keeps its order (fog and overcast darker and flatter than clear; snow the only mostly white ground) and the clip limits.
    - Retune only the thresholds that encode the old ACES 0.85 output. Name every changed threshold, with its old and new value, in the commit message.
    - Keep `--tone-mapping aces` captures byte-identical to the pre-task default.
12. **Side-by-side evidence:** a CTest under the `gallery` label writes paired captures to `build/<dir>/verify/image-quality/`, with a summary report of checksums and statistics:
    - photo vs procedural;
    - neutral vs aces;
    - none vs msaa+smaa;
    - for the default view, the G-key ground view, a forest edge and the overview.

    If `../ez-tree/src/app/public/og-image.png` exists, the report also prints its ground-region statistics next to VTL's. The test must not fail when it is absent.

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

Record the GPU cost on a physical GPU at 1920x1080 for the default view: median and 95th-percentile frame time. Use these modes: none, msaa, smaa and msaa+smaa, with and without tone mapping. Provisional budgets are at most 0.3 ms for the tone-map pass and 1.0 ms for SMAA 1x at 1080p. Explain deviations. Do not invent results for Windows, macOS or absent hardware; the maintainer checks Windows.

## Final deliverables and definition of done

- VVE: msaa4 verified on all backends and native draws; opt-in SMAA 1x and Khronos PBR Neutral tone mapping with defaults unchanged; tests; third-party notice; README, AGENTS, `src/versions/simple/AGENTS.md`, `docs/notes/architecture.md` and `examples/FACADE_AUDIT.md` updated.
- VTL:
  - `--antialiasing`, `--textures` and `--tone-mapping`;
  - photographs packed at build time and used by default for ground, leaves and bark;
  - the procedural mode byte-identical to the old default;
  - Neutral tone mapping calibrated to the EZ-Tree reference;
  - tests and gallery;
  - `AGENTS.md`, README and the photo README updated with the final species assignment and physical scales.
- Successful Linux build and test logs, inspected captures, measured timings, and a final status summary listing changes, commands and results, artifacts and any remaining blockers.
- Do not push or publish unless the maintainer's workflow separately authorizes it.
