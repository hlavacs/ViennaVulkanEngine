# Renderer application compatibility

Updated 31 October 2026 for the implemented **approximate diffuse GI** renderer.
The GI-09 evidence in the renderer guide records the current Linux runs; the
older application matrix below retains its original forward/deferred provenance.
Windows/macOS and physical-GPU execution remain unverified.

Every graphical VVE example accepts `--renderer forward|deferred|hybrid-gi`.
The facade selection is `RendererBackend::hybrid_gi`; forward remains the default.
Selection is per engine, shared by its windows, and fixed at GPU startup.
Runtime `GiSettings` supplies independent GI/AO switches, indirect strength and
low/medium capture budgets. Disabled/zero-strength GI with AO off matches deferred.

The renderer guide is the single detailed contract for:

- [Selection, capability failure and transactional errors](deferred-renderer.md#runtime-renderer-selection), with [required formats and coverage](deferred-renderer.md#antialiasing-and-capabilities).
- [GiSettings, probe representation, initialization, scrolling and invalidation](deferred-renderer.md#approximate-diffuse-gi-gi-02).
- [Quality presets and measured costs](deferred-renderer.md#performance--presets-gi-08), including why low/medium allocate identical bytes.
- [Per-window ownership, lifetime and aggregate budgets](deferred-renderer.md#lifecycle-ownership-and-memory-limits-gi-07).
- [Native procedural passes, encoding, borrowed resources and retirement](deferred-renderer.md#native-procedural-geometry-gi-06a), including [VTL adoption](deferred-renderer.md#countryside-native-adoption-gi-06b).
- [Spatial contact AO](deferred-renderer.md#spatial-contact-ao-gi-04) and [approximation limits](deferred-renderer.md#approximation-limits).

| Backend | Native opaque | Native shadows / radiance | Forward-only surfaces |
| --- | --- | --- | --- |
| forward (default) | `forward_opaque` | Light `shadow` views | `forward_transparent` |
| deferred | Three-target `gbuffer` | Light `shadow` views | `forward_transparent` |
| hybrid_gi | Same `gbuffer`, shared GI/AO | Light/cached probe-sun `shadow`, per-face `probe_capture` | `forward_transparent`, shared irradiance sampling |

VTL links the selected VVE source/prebuilt tree directly. Its native API check
reports **patch NOT applied (no staging)**. Terrain, vegetation and stones
participate in G-buffer/shadow/probe views; water blends over opaque depth and
never becomes a probe blocker. No private renderer import or second GI implementation
is used. The existing scene and plant-generation contracts are preserved.

GI-08 measured software Vulkan only: at 720p/no-AA, low/medium with AO cost
45.447/46.182 ms median versus 6.785 ms deferred, with identical 5,373,440-byte
VMA deltas. The provisional physical-GPU +4 ms at 1080p target remains unverified.
See the guide for per-pass measurements, the 1 km countryside benchmark and
its device/access limitations.

Current regression counts, five-view/four-state countryside measurements and
artifact paths are consolidated in [GI-09 Linux acceptance](deferred-renderer.md#gi-09-linux-acceptance-31-october-2026).

GI-09's 31 October Linux run passed **62/62 Debug GI**, **31/31 deferred** and
**225/225 complete Release** VVE tests. Both VTL quick variants passed
**107/107**, including all eight GI cases; the required Release subset passed
**7/7**. Disabled/zero-strength differences remain exactly zero at 1x/MSAA4.
The actual countryside executable supplied **20 inspected PNG/HDR pairs** and
**19 passing region gates**: the detail stone gained **.0040437 linear luminance**,
while open ground retained zero GI error and a **.0002176 mean RGB AO change**.
Water remained transparent and outside opaque/shadow/probe scopes. Debug and
checked external Release synchronization validation reported zero errors.
These are llvmpipe correctness results; physical-GPU/Windows/macOS execution and
the 25 excluded slow VTL tests are not claimed.

## Earlier application verification

On Linux, build with `./build_linux.sh release`. Executables are in
`bin/release/exe`. For example:

```sh
bin/release/exe/game --renderer deferred --frames 180 --output bin/release/verify/game_deferred.png
bin/release/exe/testscene --renderer forward --frames 3 --output bin/release/verify/testscene_forward.png
ctest --test-dir build/release-linux -L example --output-on-failure
```

Use the existing `build_macos.sh` or `build_windows.cmd` workflow on those
platforms and the same executable arguments. No renderer build option exists.
The executable named `simple_forward_demo` also selects any of the three backends; its
historic name remains unchanged. `WindowSetup::renderer(RendererId)` remains a
window renderer identifier and does not select the rendering algorithm.

Each registered graphical application check has a deferred counterpart ending
in `Deferred`. Existing forward test names and capture paths are preserved;
deferred counterpart capture names add `_deferred`. Both selections execute the
same built executable. `LightShadowDebugDeferred` writes `deferred.txt` and
`deferred.png`. Bounded game runs use a stable crate random seed and simulation
step; bounded post-processing uses a stable effect time. The sibling physics
example also uses a fixed physics step for bounded captures.
The 180-frame crate-game checks pass `--verify-collection`: the first crate
drops at the player's position, then the ordinary collection code must remove
it and increase the score. This verifies resource removal after GPU rendering
has begun without changing ordinary launches.

| Application | Important rendering and runtime features | Bounded verification | Build | Deferred run | Capture location |
| --- | --- | --- | --- | --- | --- |
| `audio_demo` | Windowless audio; renderer selection/query only | `AudioDemo`, `AudioDemoDeferred` | Pass | Pass; no rendering | None: no window |
| `game` | Textured floor and falling crates, dynamic transforms, object removal, unlit sun, directional light/shadows, GUI, audio | `game`, `gameDeferred` (180 frames, `--verify-collection`) | Pass | Pass; score increased to 1 | `build/release-linux/examples/game/verify/game_deferred.png` |
| `testscene` | Textured objects, ten directional/point/spot light slots, shadows, GUI light controls | `testscene`, `testsceneDeferred` | Pass | Pass | `build/release-linux/examples/testscene/verify/testscene_deferred.png` |
| `sponza` | Imported glTF scene/materials/textures, camera clipping | `sponza`, `sponzaDeferred` | Pass | Pass | `build/release-linux/examples/sponza/verify/sponza_deferred.png` |
| `light_shadow_debug` | Directional, point and spot shadows, light markers, object removal, CPU/GPU depth checks | `LightShadowDebugExample`, `LightShadowDebugDeferred` | Pass | Pass | `build/release-linux/examples/light_shadow_debug/verify/deferred.png` |
| `simple_forward_demo` | Sample scene, native readback, multiple frames in flight | `simple_forward_demo`, `simple_forward_demoDeferred` | Pass | Pass | `build/release-linux/examples/simple_forward_demo/verify/simple_forward_demo_capture_deferred.png` |
| `hierarchy_car` | Imported glTF hierarchy, animated parent/child transforms, per-frame wheel checks, shadows | `HierarchyCarExample`, `HierarchyCarExampleDeferred` (120 frames) | Pass | Pass | `build/release-linux/examples/hierarchy_car/verify/hierarchy_car_deferred.png` |
| `postprocessing` | HDR chain, default chain and all 15 effects, textures, directional/point lighting, ImGui controls and composition | `PostProcessingCapture`, `PostProcessingCaptureDeferred`; additional default-chain captures | Pass | Pass; both chains | `build/release-linux/examples/postprocessing/verify/all_effects_deferred.png`; `bin/release/verify/renderer_apps/pp_default_deferred.png` |
| `racer` | Imported car/nature models, animated hierarchies, generated textured circuit, transparent geometry/ghost, sprite smoke/dust, sky, day/night shadows/headlights, GUI images/HUD, sound | `RacerExample`, `RacerNightExample`, `RacerMenuExample`, `RacerGhostExample`, and all four `Deferred` counterparts | Pass | Pass; all four scenarios, including 2400-frame ghost | `build/release-linux/examples/racer/verify/*_deferred.png` |
| `island_delivery` | Imported aircraft/people/vehicles, generated islands, streaming replacement/removal, animated traffic, transparent water, unlit objects, sky, shadows, MSAA4, post-processing, GUI/map, spatial audio | `IslandDeliverySmoke`, `Autopilot`, `Cockpit`, `Map`, `Airport`, `CargoLoop`, `RemoteAirport` and corresponding `Deferred` checks; additional showcase captures | Pass | Pass; all seven scenarios and showcase | `build/release-linux/examples/island_delivery/verify/*_deferred.png`; `bin/release/verify/renderer_apps/island_showcase_deferred.png` |
| Sibling `ViennaPhysicsEngine/physicsexample` | Textured rigid bodies, dynamic cloth vertices, object creation/removal, directional/point lights/shadows, GUI | `--frames 120 --cloth --capture PATH`; also `--stack --capture PATH`, each with both backends | Pass | Pass; one animated cloth and 120 stable rigid bodies | `bin/release/verify/renderer_apps/physics_{cloth,stack}_deferred.png` |

The Island Air test names all start with `IslandDelivery`. Racer's longer race
and ghost checks retain the `racer` label; build scripts skip that label, so run
`ctest --test-dir build/release-linux -L racer --output-on-failure` explicitly.
Racer checks retain the full simulation and rendering features at 640x360;
interactive launches still default to 1280x720. Use its optional `--width` and
`--height` arguments to select another capture extent.
The audio example accepts `--renderer` for consistent argument handling but
does not initialize a Vulkan window or execute a rendering pass.

The accessible workspace is
`/home/hlavacs/Dokumente/GitHub/ViennaVulkanEngine`, not the supplied Windows
path. The sibling search found one active consumer of this checkout:
`../ViennaPhysicsEngine/examples/physicsexample`. Its small CLI change lives in
that sibling repository and must accompany an application update there.
Sibling AI, geometry and agent-loop libraries contained no active VVEngine
game. Historical `../ai-runs` snapshots are archived project copies, not active
consumers of this checkout.

## Recorded verification

All matrix rows were built and executed with both selections on Linux x86_64,
Clang 18/libc++, Vulkan SDK 1.4.350.1 and Mesa 25.2.8 llvmpipe (LLVM 20.1.2,
Vulkan 1.4.318). All final deferred application captures were opened and
inspected. Every graphical deferred launch reports
`renderer=deferred gbuffer=1 deferred_lighting=1`; Island Air reports
`samples=4`. The windowless audio example only verifies selection and querying.

The final existing build workflow passed **149/149 tests**, including 38
application checks, in 69.15 seconds. The separate Racer run passed **8/8** in
243.71 seconds. The sibling passed all four 120-frame cloth/stack runs. Two
additional default-chain post-processing runs and two Island Air showcase runs
at 1280x720 also passed. The showcase captures expose the imported aircraft and
its shadow beyond the HUD. Forward checks passed for every listed scenario.

```sh
./build_linux.sh release
ctest --test-dir build/release-linux -L racer -j1 --output-on-failure
(cd ../ViennaPhysicsEngine && CMAKE_BUILD_PARALLEL_LEVEL=4 ./build_linux.sh release)
for backend in forward deferred; do
    for scene in cloth stack; do
        bin/release/exe/physicsexample --renderer "$backend" --frames 120 "--${scene}" \
            --capture "bin/release/verify/renderer_apps/physics_${scene}_${backend}.png"
    done
    SDL_AUDIO_DRIVER=dummy bin/release/exe/postprocessing --renderer "$backend" --frames 3 \
        --output "bin/release/verify/renderer_apps/pp_default_${backend}.png"
    SDL_AUDIO_DRIVER=dummy bin/release/exe/island_delivery --renderer "$backend" --frames 3 \
        --camera showcase --hidden --width 1280 --height 720 \
        --output "bin/release/verify/renderer_apps/island_showcase_${backend}.png" \
        --verify "bin/release/verify/renderer_apps/island_showcase_${backend}.csv"
done
```

The application evidence directory is `bin/release/verify/renderer_apps/`:

- `non-racer-final-detailed.log`: detailed output from the final 149-test workflow.
- `racer-final.log`: all eight final Racer checks, including backend pass diagnostics.
- `physics_*.log`, `pp_default_*.log`, `island_showcase_*.log`: additional bounded checks.
- `capture-comparisons.txt`: native C++ comparison results for **22/22** forward/deferred PNG pairs.
- `capture_compare.cpp` and `capture_compare`: the generated native PNG comparison source and executable.
- `examples-initial.log`: the earlier complete 46/46 application baseline before the final validation fixes.

The native comparison permits a mean absolute RGB error of 4 byte values and
at most 5% of pixels with any channel error above 16 byte values. This permits
sparse geometry edges and changing GUI labels while detecting broad material
or lighting regressions. All 22 pairs passed; the largest measured mean was
0.951/255 and the largest fraction above 16 was 0.165%. Most pairs had a mean
below 0.053/255. The renderer's separate native tests apply tighter scene-specific
checks for depth, normals, lights, shadows, transparency, MSAA and composition.

The final Debug deferred suite separately passed 28/28 tests with the SDK
validation layer and synchronization validation active, with no validation or
synchronization errors. It includes an actual Vulkan Profiles capability
restriction that verifies deferred selection fails while forward remains usable.

No application asset or dependency blockers remain for this Linux checkout.
Physical GPU access was unavailable to this session, so these results describe
software Vulkan. The supplied Windows path was inaccessible; Windows and macOS
builds and execution remain unverified.
