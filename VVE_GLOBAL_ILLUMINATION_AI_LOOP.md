# AI-loop task: a new VVE renderer with global illumination

## Objective and authorization

Implement a new, explicitly selectable **hybrid deferred renderer with real approximate diffuse global illumination** in ViennaVulkanEngine (VVE). Integrate ViennaTerrainLibrary (VTL) so its countryside terrain, trees, bushes, flowers, stones and water participate appropriately.

The requested result is working code, Linux builds, automated tests, inspected captures and documentation. Do not stop after a design document, an enum value, ambient occlusion, or an attractive screenshot. Ambient occlusion alone is not global illumination: the result must demonstrate indirect light and color bounce from other surfaces, including surfaces outside the main camera's view.

This task authorizes adding a renderer alongside VVE's existing rendering paths. Keep the current default and existing backends working. It does not request a replacement engine implementation, a new scene graph, or a general render-graph framework.

Use this file as the task brief supplied to AI-loop. Start in the Linux VVE checkout. Expect a sibling VTL checkout, usually:

```text
<workspace>/ViennaVulkanEngine/
<workspace>/ViennaTerrainLibrary/
```

Resolve the actual paths locally. Do not use Windows paths or copy compiled Windows artifacts. If VTL is unavailable, complete the independent VVE phases and report the VTL integration as outstanding; do not claim the entire task is complete.

## Read the current checkout before implementing

Read the applicable `AGENTS.md` and `CLAUDE.md` files in both repositories. Inspect these existing VVE sources and documents:

- `src/Types.ixx` and `src/RenderSystem.ixx`.
- `src/versions/simple/AGENTS.md`.
- `src/versions/simple/Render/Renderer.ixx` and the corresponding rendering/resource implementation files.
- `src/versions/simple/Render/RenderSystem.ixx`.
- `src/versions/simple/shaders/simple_deferred.slang` and the shared material/lighting shaders.
- `docs/deferred-renderer.md`, `docs/renderer-compatibility.md`, `docs/RenderDoc.md`.
- `tests/CMakeLists.txt`, existing backend/deferred tests and `tests/support/TestSupport.ixx`.
- `build_linux.sh` and the repository's existing AI-loop workflow.

Relevant VTL sources:

- `cmake/VTLVVENativeDraw.cmake` and `cmake/vve-native-draw.patch`.
- `examples/common/VVEBridge_Terrain*.cpp`, `VVEBridge_TerrainGpu.hpp`, `VVEBridge_Shadow.cpp`.
- `VVEBridge_Trees.cpp`, `VVEBridge_Details.cpp`, `VVEBridge_Grass.cpp`, `VVEBridge_FlowingWater.cpp` in the same directory.
- `examples/common/shaders/terrain_surface.slang`, `terrain.slang`, `trees.slang`, `grass.slang`, `details.slang`, `water.slang`.
- `examples/countryside/Tessellated.cpp`, `Options.cpp`, `PreviewWeather.hpp` and the terrain GPU tests.

### Starting facts, to recheck on Linux

The source inspected when this brief was written already has:

1. `RendererBackend::forward` and `RendererBackend::deferred`, selected through the public facade before the first rendered frame. Forward is the default. Backend selection is per engine and shared by that engine's windows.
2. A hybrid deferred path: opaque and alpha-cutout geometry into a G-buffer, deferred lighting, then forward sky/transparency/water, post-processing and GUI.
3. Three RGBA16F G-buffer attachments carrying base color/roughness, world normal/metalness and emission/AO/flags, plus D32 depth. Position is reconstructed from depth.
4. Existing no-AA and MSAA4 handling, capability checks, resize/multi-window ownership, shadow maps, resource retirement and diagnostic captures.
5. A VTL GPU preview whose custom native shaders own much of their lighting. It will not automatically receive GI merely because VVE's ordinary mesh shader changes.
6. A VTL build-time native-draw patch that stages a modified VVE copy. Do not assume that callback already exists in upstream VVE, or that it supports the new passes.

These are observations, not proof that the Linux checkout is identical. Search by symbols rather than fixed line numbers. Historical test results in documentation are not fresh verification.

## Required architecture

### Renderer selection and compatibility

- Add a genuinely selectable new renderer backend. Suggested public enum name: `RendererBackend::hybrid_gi`; suggested example CLI spelling: `--renderer hybrid-gi`. These names are **proposals to implement**, not currently existing interfaces.
- Reuse the existing deferred G-buffer, shared geometry/material code, shadows, scene resources, HDR composition and forward transparency where possible. A new renderer choice does not require duplicating the engine or all renderer code.
- Preserve existing `forward` and `deferred` choices, their defaults, public error behavior and existing tests.
- Keep facade wrappers implementation-free. Implementation details belong in the concrete renderer. Applications must not import private renderer modules.
- Preserve startup-only backend selection. GI enable/disable and quality changes may be runtime controls if their synchronization and resource replacement are explicitly implemented and tested.
- Report unsupported capabilities clearly and preserve the previous valid selection. Do not silently substitute forward rendering when the user selects the GI renderer.

### Baseline GI method

Implement **world-space irradiance probes with geometry-aware visibility**, populated by small rasterized scene captures. Supplement this with screen-space ambient occlusion for local contact detail.

This baseline must work without Vulkan ray-tracing extensions or RTX hardware. Hardware ray tracing and screen-space GI can be future improvements; neither is required to complete this task. Screen-space GI alone is insufficient for the off-camera bounce requirement.

Required probe behavior:

- Capture scene radiance from each updated probe, using the existing materials, direct lighting, shadows, emissive surfaces and sky. Capture untonemapped linear HDR radiance, without camera fog, post-processing or GUI.
- Include opaque and alpha-cutout geometry. Respect leaf/grass masks and double-sided materials; do not turn foliage cards into solid rectangles.
- Integrate directional incoming light into a compact diffuse representation, such as RGB spherical harmonics. Document the representation's units and the cosine/normalization factors so irradiance is not multiplied by pi, albedo or exposure twice.
- Support at least one diffuse bounce. Probe capture should use direct lighting, emission and sky, without feeding an accumulating copy of its own previous indirect result back into itself. Multiple-bounce propagation is out of scope for the first complete version.
- Store/use directional depth or equivalent visibility information to suppress leakage through walls, terrain and tree trunks. Trilinear interpolation of unoccluded RGB alone is insufficient.
- Handle probes inside geometry and newly introduced probes explicitly, using relocation/inactivation/validity as appropriate. Invalid probes must not introduce black holes or bright flashes.
- Use a bounded, world-anchored local volume or a small number of cascades for a large countryside scene. Snap scrolling to probe cells, preserve overlap and prioritize newly exposed/nearby/invalid probes.
- Update a bounded subset of probes per frame. Do not recapture the entire scene for every probe on every frame or allocate a region-sized dense volume for a kilometer-scale terrain.
- React to sun, sky/weather, emissive, geometry and transform changes. Define invalidation and convergence behavior, including camera teleports and scene replacement.
- Blend to the documented existing environment-light fallback outside valid probe coverage, without a hard lighting boundary. Untouched probe storage must never be sampled as valid data.

A reasonable first implementation is a small near volume plus a coarser volume, low-resolution cubemap captures and a fixed per-frame update budget. Choose actual counts, face sizes and spacing from measurements. Record the settings and their memory/update costs; expose a small useful quality interface rather than many undocumented knobs.

### Ambient occlusion and temporal stability

- Compute depth/normal-aware screen-space AO, with filtering that preserves silhouettes and avoids halos. Half-resolution evaluation with edge-aware upsampling is acceptable.
- Apply AO to appropriate indirect illumination. Do not indiscriminately multiply direct light, emissive output, sky, fog, GUI or transparent water by AO.
- Account for existing material AO and probe visibility; avoid stacking multiple full-strength occlusion terms into unnaturally black vegetation.
- If a screen-space effect uses temporal history, implement the required reprojection data and reset/reject history on disocclusion, camera cuts, resize, scene replacement and quality changes.
- If a stable spatial method suffices, do not add temporal machinery merely to satisfy a checklist. Probe history still needs well-defined invalidation and convergence.

### Lighting and pass order

Use an explicit pass sequence consistent with the existing renderer, for example:

1. Prepare scene updates and capture budgets; render required shadow maps.
2. Update the selected probes and integrate their radiance/depth data.
3. Render opaque and alpha-cutout geometry into the G-buffer and depth.
4. Evaluate/filter AO and other required visibility data.
5. Shade opaque surfaces using direct light, probe indirect light, emission and existing specular/environment response.
6. Render sky and forward transparency/water using the preserved depth and suitable shared indirect-light sampling.
7. Apply the existing atmosphere/post-processing pipeline at its correct stage, then GUI and presentation.

Adjust dependencies if the implementation needs another order, and document why. Do not introduce a generic render graph.

Preserve energy and color-space conventions. Replace the corresponding constant diffuse ambient contribution when probes supply it; do not simply add GI over an already equivalent ambient term. Metals must not acquire an incorrect diffuse bounce term. Keep direct sun and emissive brightness stable when toggling indirect light.

Provide independent GI and AO toggles plus an indirect-strength control for diagnosis. With GI strength zero and AO disabled, the new backend should match the existing deferred result within documented numerical tolerance.

### MSAA, resources and multiple windows

- Preserve no-AA and MSAA4 correctness. Reuse the existing per-coverage-sample material/depth handling; do not average unrelated normals/depths across a silhouette and treat that as one surface.
- A lower-resolution/single-sample AO representation may be used, but its edge selection and application must be explicit and tested.
- Own screen-sized buffers/history per window. Define whether probe volumes are shared by nearby views or allocated per view, and enforce a documented total budget for distant windows.
- Use the existing frame-slot/fence and retirement rules. Do not overwrite a probe atlas, descriptor or history image still being read by an in-flight frame.
- Implement correct Vulkan image layouts, barriers and access dependencies, including probe capture-to-sampling and history read/write dependencies.
- Avoid new steady-state device-wide idle waits, synchronous CPU readbacks, or per-frame scene reuploads. Readback for tests/debugging must be opt-in and bounded.
- Release partial allocations after failed initialization. Test resize, minimization/resume, teardown, scene replacement and repeated create/destroy cycles.

## VTL integration is part of completion

Establish a generic, supported VVE integration interface for procedural geometry. Extend/adopt the current native-draw mechanism only as needed; do not introduce VTL types into VVE.

The interface must let VTL participate in the relevant opaque G-buffer, shadow and probe-capture passes, and let forward-only surfaces sample the shared indirect lighting. Pass type, view/camera, attachment formats/sample counts, resource lifetime and shader data conventions must be explicit. Use public contracts instead of reaching into VVE internals.

In particular:

- Terrain, stones, tree trunks, shrubs, flowers, grass and masked foliage must contribute compatible material/normal/depth data and receive GI.
- Probe and shadow culling must use the capture view, not only the main camera's visible-instance list. Off-screen geometry still needs to block and bounce light.
- Choose bounded capture LODs for dense vegetation while preserving useful crown coverage and leaf cutouts. Validate the approximation against actual silhouettes.
- Do not leave VTL's previous baked-in ambient term active on top of the engine's GI term. Factor common lighting/sampling where appropriate.
- Transparent water must compose correctly with opaque depth and indirect/environment lighting. It must not become an opaque probe blocker simply to make integration convenient. Full refractive/volumetric light transport is not required.
- Preserve the existing river continuity, water surface appearance, varied stones, visible shrubs/flowers and G-key 1.8 m ground-camera mode. Do not modify terrain generation, hydrology, collision or plant-growth contracts to fake lighting results.
- Reconcile `VTLVVENativeDraw.cmake` and its patch with the supported VVE API. Avoid applying the same hook twice or maintaining a second GI implementation inside a generated engine copy. Modify source/build configuration, never hand-edit generated build trees.
- Expose the new renderer in the countryside application through its public options. A simple GI/AO toggle is useful for comparisons. Existing defaults remain compatible.

If the native hook already changed on Linux, adapt this phase to the current implementation. Do not carry a stale Windows patch forward blindly.

## AI-loop execution phases

Use focused commits following the repository's established controller/worker workflow: the worker authors tests; the controller builds, runs, inspects evidence and requests repairs. Preserve unrelated user changes. Update this task's progress in a separate status document or the existing loop log, without rewriting unrelated historical tasks.

| ID | Work | Completion gate |
| --- | --- | --- |
| GI-00 | Audit current renderer, VTL hook, Linux toolchain and tests; record baseline captures and timings. | Paths/capabilities verified; baseline failures distinguished from new regressions; implementation plan recorded. |
| GI-01 | Add the selectable GI backend and facade/configuration contract, reusing deferred resources. | Selection/errors tested; zero-GI/no-AO matches deferred; old defaults unchanged. |
| GI-02 | Add bounded raster probe capture and radiance integration. | Controlled scene demonstrates real diffuse color bounce, with correct units and no feedback growth. |
| GI-03 | Add probe visibility, invalid-probe handling, scrolling and update scheduling. | Wall leakage, off-camera contribution, teleports and dynamic-light convergence tested. |
| GI-04 | Add contact AO and edge-aware filtering; implement history only if used. | AO has local geometric effects without global darkening, halos or stale history. |
| GI-05 | Finish HDR lighting, alpha-cutout, transparency/water and MSAA integration. | Materials, energy response, silhouettes and forward composition pass. |
| GI-06 | Add/adapt the supported procedural-geometry interface and VTL bridge integration. | Actual countryside surfaces contribute to and receive GI; VTL patch/build workflow remains reproducible. |
| GI-07 | Harden resize, multi-window, capability failure, memory limits and resource lifetime. | Validation/synchronization tests and bounded-memory checks pass. |
| GI-08 | Tune performance and quality on Linux; inspect controlled and countryside captures. | Measured budgets, useful presets and documented limitations; no visual-only acceptance. |
| GI-09 | Complete regression runs, documentation, example controls and final evidence. | All required acceptance criteria below met, or unresolved blockers reported explicitly. |

Phases may be split into smaller commits. Do not claim completion at an intermediate stage. If a requirement is already implemented, verify and reuse it. When a test fails, diagnose the image/data/validation output rather than relaxing the test to match the implementation.

## Required acceptance tests

Register native C++ tests with CTest using the existing support module and conventions. Use deterministic scenes, fixed exposure/cameras/seeds and controlled probe warm-up. Do not introduce Python tests.

1. **Selection and fallback:** forward/deferred/GI are distinct choices; invalid values and unsupported capabilities fail transactionally; independent engines retain independent settings; late backend changes retain the existing contract.
2. **Disabled equivalence:** GI zero + AO off matches the original deferred backend, including opaque materials, emission, sky and transparency. Use the existing comparison tolerances where applicable.
3. **Diffuse bounce:** illuminate a colored wall beside a neutral receiver that has no direct line to the light. GI must change the receiver's measured linear-light color in the expected direction. Recoloring/removing the wall must change that bounce. Raising a uniform ambient constant cannot pass this test.
4. **Off-camera contributor:** keep the receiver in view while the contributing wall is outside the main view. The bounce remains present; camera framing alone must not erase it.
5. **Occlusion and leakage:** insert an opaque blocker or separating wall; indirect light on the protected side falls substantially. Include probes near/inside walls and terrain. Measure explicit regions, not only whole-image averages.
6. **Sky visibility and contact:** compare open ground, ground beneath a canopy and stone/ground contacts. AO affects appropriate local indirect response; direct-lit open areas and unlit/emissive references remain stable.
7. **Dynamics:** change sun direction/intensity, sky/weather, emissive brightness and an object's position. Verify the expected direction of change and convergence within a documented update budget. Include a camera teleport and scene replacement.
8. **Foliage/materials:** masked leaves retain holes and double-sided behavior in main/probe/shadow views. Normals, roughness, metalness, material AO and linear/sRGB conventions remain correct.
9. **Transparency/water:** opaque depth and transparent ordering remain correct; no opaque water blocks, missing rivers, double lighting or GI applied after GUI/tonemapping.
10. **AA and temporal behavior:** exercise no-AA and MSAA4 silhouettes. Where history exists, test stationary convergence, motion, disocclusion, resize and camera cuts, including reset counters.
11. **Lifecycle and isolation:** multiple windows/cameras, minimize/resume, repeated setup/teardown, failed initialization, quality changes and scene replacement. No synchronization validation errors, leaked resources, history shared between unrelated views or unbounded allocations.
12. **Actual VTL:** paired captures of the same countryside seed/camera/weather with GI off/on and AO off/on, including ground height, a forest edge, stones/bushes/flowers, a river/lake and a higher overview. Check that procedural geometry participates rather than merely being drawn over a GI-lit VVE background.

For image tests, use identical direct lighting, exposure, tonemapping, animation time and scene data between comparisons. Derive thresholds from a controlled fixture or reference and document them. A changed-pixel count alone does not establish correct bounced lighting. Inspect linear/HDR measurements as well as final PNGs to avoid tonemapping masking errors.

Provide diagnostic modes for indirect-only light, AO, probe positions/validity/coverage and G-buffer normals/depth. Emit small machine-readable receipts such as:

```text
renderer.backend=hybrid-gi
gi.enabled=1
gi.probes.valid=...
gi.probes.updated=...
gi.coverage.invalid=...
gi.gpu_bytes=...
gi.capture_ms=...
gi.integrate_ms=...
gi.lighting_ms=...
ao.gpu_ms=...
gi.history_resets=...
validation.errors=0
```

Names can follow existing conventions. Values must be measured, not hard-coded success indicators. Timing fields should distinguish unavailable GPU timestamps from genuine zero cost.

## Linux build and verification

Use the checked-in build scripts and the checkout's required matching Clang/libc++/module-scanner toolchain, CMake/Ninja, Vulkan SDK and vcpkg triplet. Do not force the toolchain versions from another machine or reuse incompatible module files.

Existing VVE entry points, run from its root:

```bash
./build_linux.sh debug
./build_linux.sh release
ctest --test-dir build/debug-linux -N
ctest --test-dir build/debug-linux -L gi --output-on-failure
ctest --test-dir build/debug-linux -L deferred --output-on-failure
ctest --test-dir build/release-linux --output-on-failure
```

The `gi` label is new: register it for the tests implemented by this task and check that the filtered run actually contains tests. The scripts already run their configured tests; avoid redundant complete runs unless changes or the repository workflow justify them. Use focused runs during iteration and a complete final regression run.

Build VTL with the corresponding variant after resolving its supported VVE/native-hook integration:

```bash
cd ../ViennaTerrainLibrary
./build_linux.sh debug --quick
./build_linux.sh release --quick
ctest --test-dir build/release-linux -R 'ExampleCameraControls|TerrainTessellationGpu|TerrainLandscapeGpu|TerrainDetailsContracts|TerrainTreesGpu|test_terrain_water' --output-on-failure
```

Discover the actual configuration's test registrations. Run relevant slower integration tests for affected behavior; `--quick` does not replace them or prove every VTL test passed. Add GI-specific VTL tests to the normal registered workflow.

Enable the installed Khronos validation layer and synchronization validation for Debug runs according to current repository instructions. Use a working Vulkan presentation setup or the project's supported test harness. A skipped graphics test or absence of a display is not a successful GI validation.

Run small correctness scenes on software Vulkan if available, but measure performance on a physical GPU. Record CPU, GPU, driver, Vulkan version, resolution, sample count, scene, quality settings, warm-up and median/high-percentile timings. Report incremental GPU cost and memory relative to the existing deferred renderer. Suggested initial optimization targets are an additional roughly 4 ms and 64 MiB at 1080p on the declared physical test GPU; these are provisional goals, not hardware-independent promises. Explain deviations and provide a usable lower-cost preset.

Do not invent results for Windows/macOS or absent hardware. Keep portable compilation and capabilities in mind, preserve existing platform behavior, and record precisely what was and was not tested.

## Final deliverables and definition of done

- A new runtime-selectable VVE GI renderer, with a supported public configuration interface and preserved old renderers.
- Real one-bounce diffuse indirect lighting, geometry-aware probe visibility and appropriate contact AO, with bounded update/memory costs.
- Working integration of the actual VTL GPU countryside geometry and water.
- Deterministic registered CPU/GPU regression tests, including off-camera color bounce and light-leak suppression.
- Successful Linux build/test logs, validation results, inspected comparison captures and measured performance/memory receipts.
- Documentation of renderer selection, quality controls, probe initialization/convergence, capability requirements, ownership/lifetime and approximation limits. Update the existing renderer compatibility documentation and relevant AGENTS/API documentation with the implemented behavior.
- Focused commits and a final status summary listing changes, commands/results, artifacts and any remaining blockers. Preserve unrelated work; do not publish or push unless separately authorized by the maintainer's workflow.

The final explanation must accurately call this **approximate diffuse GI**. It need not provide path-traced reflections, refraction, caustics, unlimited-range transport or unlimited bounces. It must provide visible, measurable bounced lighting and solve the VTL integration, rather than merely darkening crevices or increasing ambient brightness.
