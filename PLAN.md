# Implementation Plan: PCF Smoothing + Cascaded Shadow Maps for the Simple Engine

Read AGENTS.md and src/versions/simple/AGENTS.md first.

## Context

Target: the **simple forward renderer** in `src/versions/simple/` (C++23 modules, Vulkan 1.3 dynamic rendering, Slang shaders). Relevant files:

- `src/versions/simple/shaders/simple_forward.slang` — all vertex/fragment entry points, incl. shadow-pass vertex shaders (`shadowVertexMainDir`, `shadowVertexMainSpot`, `shadowVertexMainPoint`) and all shadow sampling in `fragmentMain`.
- `src/versions/simple/Vulkan/Shadow.ixx` — `ShadowMap` struct: owns the D32 depth image (fixed `resolution = 1024`, N array layers, per-layer 2D views in `ownedLayerViews`), the sampler, and the depth-only pipeline (`createPipeline`).
- `src/versions/simple/Render/RendererShadowPrep.ixx` — `ForwardRendererShadowPrep::prepareShadowFrame()` builds all CPU light-space matrices per frame, incl. one ortho matrix per directional light into `frame.dirLightViewProjArray`.
- `src/versions/simple/Render/RendererDraw.ixx` — computes `directionalShadowCenter`/`directionalShadowExtent` from visible object origins, calls `prepareShadowFrame`, fills `FrameUniforms`, and records one depth pass per active directional light into `dirShadowArray` layers (search `dirShadowArrayPassCount`), passing the layer index via push constant `dirLightIndex`.
- `src/versions/simple/Render/RendererResources.ixx` — creates `shadowMap` (legacy single), `dirShadowArray` (`kMaxDirectionalLights` layers), `spotShadowArray`, `pointShadowArray`, and writes their descriptors (`writeDirShadowArray` etc., bindings 4–7 of set 0).
- `src/versions/simple/Render/RendererDebug.ixx` / debug sample plumbing — CPU/GPU shadow depth diagnostic samples (`fillDirectionalShadowGpuDepthSamples` etc.) that mirror the shader's sampling math and will break if layer layout changes. Must be updated in step 2.7.
- Constants `kMaxDirectionalLights` (=10), `kMaxShadowedSpotLights`, `kMaxShadowedPointLights` are defined in the engine types module (`import VEEngine.Types`) and mirrored as `static const int` at the top of `simple_forward.slang`. Both places must stay in sync.

### Why the current shadows show artifacts

1. The fragment shader samples raw depth with a plain `Sampler2DArray.Sample().r` and does one hard binary compare: `dirLightNdc.z - 0.001 > dirStoredDepth ? 0.35 : 1.0`. No filtering → stair-stepped edges at shadow-texel granularity. Worse, the sampler uses `VK_FILTER_LINEAR` on raw depth, so depth values get averaged *before* the compare, which produces incorrect edge texels.
2. One 1024² map covers the whole fitted scene extent (`directionalShadowExtent` grows with the scene), so one shadow texel covers a lot of world area → blocky, low-resolution shadows (perspective aliasing).
3. Constant compare bias (0.001) with slope-scaled rasterizer bias explicitly zeroed in `ShadowMap::createPipeline` (`depthBiasConstantFactor = 0`, `depthBiasSlopeFactor = 0`) → acne on sloped surfaces vs. peter-panning tradeoff. `cullMode = NONE` in the shadow pass aggravates acne.
4. The directional ortho is refit every frame from object origins with no texel snapping → shadow edges shimmer/swim when anything moves.

## Goal

Phase 1: hardware + software PCF ("smoothing") and correct biasing for directional and spot shadows. Phase 2: cascaded shadow maps (CSM) for directional lights. Phase 3: validation. Do phases in order; Phase 1 stands alone and must not depend on Phase 2.

---

## Phase 1 — Comparison-sampler PCF and bias fixes

### 1.1 Make the shadow sampler a comparison sampler (`Shadow.ixx`)

In `ShadowMap::create`, change the `VkSamplerCreateInfo`:

- `compareEnable = VK_TRUE`, `compareOp = VK_COMPARE_OP_LESS_OR_EQUAL` (fragment is lit when its biased reference depth ≤ stored depth; the shadow pass stores nearest depth with `VK_COMPARE_OP_LESS` and clear = 1.0).
- Keep `magFilter/minFilter = VK_FILTER_LINEAR` — with `compareEnable` this becomes hardware 2×2 PCF (correct: compare per tap, then average), instead of the currently incorrect depth-averaging.
- Change address mode to `VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER` with `borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE`, so samples outside the map compare as "far" → lit. The explicit `insideDirShadowMap`/`insideSpotShadowMap` UV checks in the shader can then be reduced to the depth-range check only.

Note `D32_SFLOAT` with linear+compare filtering is universally supported on desktop hardware; no feature query needed (optionally assert `vkGetPhysicalDeviceFormatProperties` reports `VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT` for D32).

### 1.2 Switch the shader to comparison sampling (`simple_forward.slang`)

- Change declarations: `Sampler2DArray dirShadowArray` → `Sampler2DArrayShadow dirShadowArray`; same for `spotShadowArray` and `pointShadowArray`. (Descriptor bindings and C++ descriptor writes don't change — still combined image samplers.)
- Replace each `X.Sample(float3(uv, layer)).r` + manual compare with `X.SampleCmpLevelZero(float3(uv, layer), refDepth)`, where `refDepth = ndc.z - bias`. The result is already in [0,1] (fraction of taps lit).
- Convert the hard `? 0.35 : 1.0` into `lerp(0.35, 1.0, pcf)` so partial coverage produces the intermediate values (keep the 0.35 floor semantics).

### 1.3 Add a small software PCF kernel on top

Write a helper used by directional and spot paths:

```hlsl
float pcfShadow(Sampler2DArrayShadow map, float2 uv, float layer, float ref)
{
    const float texel = 1.0 / float(kShadowMapResolution); // pass resolution as a shader constant
    float sum = 0.0;
    [unroll] for (int y = -1; y <= 1; ++y)
        [unroll] for (int x = -1; x <= 1; ++x)
            sum += map.SampleCmpLevelZero(float3(uv + float2(x, y) * texel, layer), ref);
    return sum / 9.0;
}
```

3×3 taps × hardware 2×2 = effective 6×6 footprint; that is enough smoothing for this engine. Add `static const int kShadowMapResolution = 1024;` mirroring `ShadowMap::resolution` (or pass it in `FrameUniforms` to avoid a second hardcoded constant).

### 1.4 Fix biasing

- In `ShadowMap::createPipeline` set `depthBiasConstantFactor = 1.25F`, `depthBiasSlopeFactor = 1.75F` (slope-scaled bias handles glancing angles where constant bias fails). Keep `depthBiasEnable = VK_TRUE`.
- Reduce the shader-side constant compare bias for directional shadows from `0.001` to `~0.0005` (it's now only a safety net; the rasterizer bias does the real work). Keep `perspectiveShadowBias()` for spot/point as is initially.
- Keep `cullMode = VK_CULL_MODE_NONE` (the scene contains open geometry like the floor plane; front-face culling would break it).
- Tune on the test scene: if acne remains on sloped surfaces raise slopeFactor toward 3–4; if shadows visibly detach from object bases ("peter-panning") lower constant factor first.

### 1.5 Update the CPU debug-sample mirrors

The renderer keeps CPU-side shadow depth diagnostic samples that replicate the shader compare (`shadowCompareBias`, `RenderShadowDepthSample` fills in `RendererDraw.ixx`, and `fillDirectionalShadowGpuDepthSamples` / spot / point equivalents). Update the mirrored bias constants; where they replicate the binary compare, either replicate the center tap only or mark the factor as approximate. Grep for `shadowCompareBias` and `kDirectionalShadowCompareBias`.

**Phase 1 acceptance:** shadow edges show a smooth 2–3 texel gradient instead of hard stair-steps; no acne on the floor or on sloped/curved surfaces at glancing light angles; no detached shadows; spot and point shadows still correct; all examples (`examples/light_shadow_debug`, `examples/game`) build and run.

---

## Phase 2 — Cascaded shadow maps for directional lights

### 2.0 Design decisions (follow these; don't improvise)

- `kNumShadowCascades = 4`.
- Reduce `kMaxDirectionalLights` from 10 to **4** (update both the C++ constant and the slang mirror). Rationale: the dir shadow array becomes `kMaxDirectionalLights × kNumShadowCascades` layers; 16 layers × 1024² × 4 B = 64 MB is fine, 40 × 4 would not be.
- Layer layout: flattened, `layer = packedDirIndex * kNumShadowCascades + cascade`. `FrameUniforms.dirLightViewProjArray` becomes size `kMaxDirectionalLights * kNumShadowCascades` (=16, larger than the old 10 — keep C++ mirror struct byte-identical to the slang struct; both must be edited together, including any `static_assert` on sizeof).
- Cascade split scheme: practical split, λ = 0.5 blend of uniform and logarithmic, over `[cameraNear, shadowDistance]` with `shadowDistance = 60.0` as a new named constant (clamped to camera far if smaller).
- Stabilization: bounding-sphere fit + texel snapping (details in 2.2). This is mandatory — without it cascades shimmer on every camera move and the result looks worse than the status quo.
- Keep the legacy single `shadowMap`/`lightViewProj` path and spot/point shadows untouched by this phase.

### 2.1 Split computation (new code in `RendererShadowPrep.ixx`)

Add to `ForwardRendererShadowFrame`: `Vec4 cascadeSplitsFar{};` (view-space far distance of cascade i in component i) and mirror it in `FrameUniforms` (C++ and slang) plus a `float4 cascadeSplits;` in the slang struct at the matching offset.

```text
for i in 1..kNumShadowCascades:
    f = i / kNumShadowCascades
    uniformSplit = near + (shadowDistance - near) * f
    logSplit     = near * pow(shadowDistance / near, f)
    split[i-1]   = lerp(uniformSplit, logSplit, 0.5)
```

`prepareShadowFrame` needs the camera view matrix, vertical FoV, aspect, and near plane — extend its signature (the caller in `RendererDraw.ixx::drawFrame` has all of these where it builds `FrameUniforms.view`/`projection`).

### 2.2 Per-cascade light matrix with stabilization

For each enabled directional light and each cascade `c` with sub-range `[splitNear, splitFar]`:

1. Compute the 8 world-space corners of the camera sub-frustum (from camera position/orientation + FoV/aspect at the two split distances — derive directions from the inverse view matrix; the engine's `Simple.Math` module has `lookAt`, `multiply`, etc.).
2. Compute the bounding **sphere** of those 8 corners (center = average or minimal enclosing approximation; radius = max distance to corners). Using a sphere, not an AABB, makes the ortho extent invariant under camera rotation → no rotational shimmer.
3. Round radius up: `radius = ceil(radius * 16.0) / 16.0`.
4. Light view: `lookAt(sphereCenter - lightDir * (radius + zBackoff), sphereCenter, up)` with `zBackoff = 40.0` (pulls the near plane toward the light so casters behind the frustum slice still cast; make it a named constant). Use the same up-vector convention as the existing code (`{0,1,0}`).
5. Ortho: `orthoVulkan(-radius, radius, -radius, radius, 0.1, 2*(radius + zBackoff))` — use the existing `orthoVulkan` helper (it handles the Vulkan Y-flip; do not hand-roll).
6. **Texel snapping:** transform the world origin by `proj*view`, compute its offset in shadow-texel units (`shadowMapExtent = 2*radius`, `texelSize = shadowMapExtent / ShadowMap::resolution`), round the ortho translation so the light-space origin moves only in whole-texel increments. Standard formulation: take `shadowOrigin = (proj*view) * (0,0,0,1) * resolution/2`, `roundedOrigin = round(shadowOrigin)`, `offset = (roundedOrigin - shadowOrigin) * 2/resolution` applied to the projection matrix's translation column (x, y only).
7. Store into `frame.dirLightViewProjArray[packedDirIndex * kNumShadowCascades + c]` and record per-cascade metadata rows in `renderer.shadowLightMeta` (one row per cascade layer, `light_type = 3` or reuse the directional type with `first_layer` set accordingly — follow the existing spot/point metadata pattern).

Replace the existing directional block in `prepareShadowFrame` (the one using `lightCenter`/`lightExtent`) with this. The `directionalShadowCenter`/`directionalShadowExtent` fitting logic in `RendererDraw.ixx` (object-origin min/max loop) becomes unnecessary for cascades — remove its use for the cascaded path but leave the legacy `lightViewProj` computation untouched.

### 2.3 Resource creation (`RendererResources.ixx`)

Change `renderer.dirShadowArray.create(..., kMaxDirectionalLights)` to `create(..., kMaxDirectionalLights * kNumShadowCascades)`. `ShadowMap` already creates per-layer views and a whole-array sampling view generically — no changes needed in `Shadow.ixx` beyond Phase 1.

### 2.4 Depth-pass recording (`RendererDraw.ixx`)

In the directional shadow pass section (search `dirShadowArrayPassCount`): the pass count becomes `activeDirectionalShadowPassCount * kNumShadowCascades`, iterating `layer = dirIndex * kNumShadowCascades + cascade`. Attach `renderer.dirShadowArray.ownedLayerViews[layer]`, keep the existing per-layer barrier pattern, and pass `layer` as the `dirLightIndex` push constant — `shadowVertexMainDir` already indexes `frame.dirLightViewProjArray[object.dirLightIndex]`, so with the flattened array it needs **no change**. Also update the earlier clear-all-layers loop that iterates `dirShadowArrayLayerCount` (it derives from `ownedLayerViews.size()`, so it likely adapts automatically — verify).

### 2.5 Fragment-shader cascade selection (`simple_forward.slang`)

In the directional loop of `fragmentMain`:

1. Compute view-space depth once before the loop: `float viewDepth = -mul(frame.view, float4(input.worldPos, 1.0)).z;` (verify the sign convention against the engine's view matrix — camera looks down −Z in view space with the existing `lookAt`).
2. Select cascade: first `c` in `0..kNumShadowCascades-1` with `viewDepth <= frame.cascadeSplits[c]`; fragments beyond the last split are fully lit (skip shadowing).
3. Sample `dirShadowArray` at layer `float(i * kNumShadowCascades + c)` with the Phase 1 `pcfShadow` helper, using the matching matrix `frame.dirLightViewProjArray[i * kNumShadowCascades + c]`.
4. Scale the constant compare bias per cascade by texel world-size ratio: `bias_c = baseBias * (cascadeRadius[c] / cascadeRadius[0])` — pack the per-cascade world extents into one more `float4` in `FrameUniforms` (`cascadeExtents`), or simply multiply baseBias by `(c + 1)` as a first approximation.
5. Optional but recommended: blend between cascade `c` and `c+1` when `viewDepth` is within the last 10% of split `c` (`lerp` of the two PCF results) to hide seams.
6. Add a compile-time debug switch (`static const bool kDebugCascadeTint = false;`) that tints the fragment by cascade index (red/green/blue/yellow) for visual verification.

### 2.6 Uniform-struct synchronization

`FrameUniforms` exists twice: slang struct in `simple_forward.slang` and a C++ mirror (grep for `dirLightViewProjArray` in `src/versions/simple/` — it's built in `RendererDraw.ixx` from `ForwardRendererShadowFrame`). Field order, array sizes, and padding must match byte-for-byte (std140: `float4x4` arrays and `float4` arrays are safe; keep the `uint + uint3` padding pattern at the end intact). Add the new fields (`cascadeSplits`, optionally `cascadeExtents`) at the same position in both. If a `static_assert(sizeof(...))` exists, update it.

### 2.7 Update debug/diagnostic plumbing

`directionalShadowDepthSampleStorage` and `fillDirectionalShadowGpuDepthSamples` reference `dirLightViewProjArray[0]` and layer 0 — with cascades, pin them to cascade 0 of light 0 (layer 0 still exists, matrix index 0 still exists, so minimal changes), but re-check any assert/consistency test comparing CPU vs GPU samples. Update `examples/light_shadow_debug` expectations if it asserts on layer counts or matrix values. Also check `examples/FACADE_AUDIT.md` / tests under `tests/` for anything asserting `kMaxDirectionalLights == 10`.

**Phase 2 acceptance:** with the debug cascade tint enabled, cascade boundaries are visible and roughly follow distance from camera; near-camera shadow edges are visibly sharper than pre-CSM at the same 1024² resolution; panning/rotating the camera produces no edge shimmer on static geometry; no visible seam artifacts at cascade boundaries (with blending on); acne/peter-panning still absent in all cascades; all examples build and run on macOS (`build_macos.sh`) and the other platforms' scripts still configure.

---

## Phase 3 — Validation checklist

1. Run `examples/light_shadow_debug`: verify directional, spot, and point shadows all render; verify CPU/GPU debug depth samples still agree where asserted.
2. Visual tests with the cascade tint: (a) stand close to a caster — edge should be smooth and high-res; (b) orbit the camera — no shimmering; (c) look along the light direction at a glancing surface — no acne; (d) verify objects *behind* the camera still cast into the view (zBackoff working).
3. Toggle counts: 0 directional lights (no crash, loop skipped), max directional lights, lights toggled at runtime (`enabled` flag) — packed indices and layers must stay consistent.
4. Validation layers on: no sync or usage errors from the added cascade passes/barriers.
5. Confirm shader recompilation is hooked up: the `.slang` → SPIR-V build step must regenerate the shadow and forward SPIR-V (check `src/versions/simple/CMakeLists.txt` for the slang compile targets and the entry-point names, since `shadowVertexMainDir` is looked up by name in `createPipeline`).

## Explicit non-goals

Point-light cascade support (meaningless), variance/moment shadow maps, cascade caching across frames, and any change to the V2 renderers (`VERendererShadow11` etc.) — this plan targets `src/versions/simple/` only.
