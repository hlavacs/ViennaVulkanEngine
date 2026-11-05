/** @file @brief Public shared E/pi sampler for native forward surfaces; no engine descriptor/uniform ABI dependency.
 * Define VVE_IRRADIANCE_ORIGIN (float4), EYE (float3), VISIBILITY and CASCADES from NativeDrawContext.
 * Two sampled images need no sampler. Override the binding/set macros before inclusion when necessary.
 * Returned RGB is coverage-premultiplied linear E/pi; apply diffuse albedo, material AO and strength once.
 * Replace the same coverage*strength share of existing diffuse ambient; direct, emission and specular stay separate.
 */
#include "vve_irradiance.h"
#ifndef VVE_IRRADIANCE_SET
#define VVE_IRRADIANCE_SET 0
#define VVE_IRRADIANCE_NEAR_BINDING 0
#define VVE_IRRADIANCE_COARSE_BINDING 1
#endif
/// @brief Finite normalization shared by visibility bias and surface-to-probe weighting.
float3 probeNormalizeOrZero(float3 v) {
    const float lengthSquared = dot(v,v);
    return lengthSquared > 1.0e-12 ? v * rsqrt(lengthSquared) : float3(0.0);
}
[[vk::binding(VVE_IRRADIANCE_NEAR_BINDING, VVE_IRRADIANCE_SET)]] Texture2D<float4> probeSH;
[[vk::binding(VVE_IRRADIANCE_COARSE_BINDING, VVE_IRRADIANCE_SET)]] Texture2D<float4> coarseProbeSH;
/// @brief One storage convention for both cascades; uninitialized rows always have zero validity.
float4 probeLoad(int3 texel, uint cascade) { return cascade == 0 ? probeSH.Load(texel) : coarseProbeSH.Load(texel); }
/// @brief Nonzero sign keeps octahedral poles and edge folds well-defined.
float2 octSign(float2 p) { return float2(p.x >= 0 ? 1 : -1, p.y >= 0 ? 1 : -1); }
/// @brief Maps a unit world direction to the signed octahedral square.
float2 probeOctEncode(float3 d)
{
    d /= max(abs(d.x) + abs(d.y) + abs(d.z), 1e-6);
    return d.z >= 0 ? d.xy : (1 - abs(d.yx)) * octSign(d.xy);
}
/// @brief Inverse octahedral map shared with the moment integration pass.
float3 probeOctDecode(float2 p)
{
    float3 d = float3(p, 1 - abs(p.x) - abs(p.y));
    if (d.z < 0) d.xy = (1 - abs(d.yx)) * octSign(d.xy);
    return normalize(d);
}
/// @brief Loads a flattened moment texel, folding bilinear taps across the octahedron's seams.
float2 probeMoment(int2 texel, uint probe, uint cascade)
{
    const int side = VVE_PROBE_VISIBILITY_SIDE;
    if (texel.x < 0 || texel.x >= side) { texel.x = texel.x < 0 ? -texel.x - 1 : 2 * side - texel.x - 1; texel.y = side - texel.y - 1; }
    if (texel.y < 0 || texel.y >= side) { texel.y = texel.y < 0 ? -texel.y - 1 : 2 * side - texel.y - 1; texel.x = side - texel.x - 1; }
    return probeLoad(int3(VVE_PROBE_SH_COEFFICIENTS + texel.x + side * texel.y, probe, 0), cascade).xy;
}
/// @brief Chebyshev's upper bound rejects points behind the radial mean; cubing suppresses its light-leaking tail.
float probeVisibility(uint probe, float3 offset, uint cascade)
{
    const float distance = length(offset);
    const float2 pixel = (probeOctEncode(offset / max(distance, 1e-6)) * .5 + .5) * VVE_PROBE_VISIBILITY_SIDE - .5;
    const int2 texel = int2(floor(pixel));
    const float2 f = frac(pixel);
    const float2 moments = lerp(lerp(probeMoment(texel, probe, cascade), probeMoment(texel + int2(1,0), probe, cascade), f.x),
        lerp(probeMoment(texel + int2(0,1), probe, cascade), probeMoment(texel + int2(1,1), probe, cascade), f.x), f.y);
    const float scale = VVE_PROBE_SCALE(cascade);
    const float variance = max(moments.y - moments.x * moments.x, 1e-5 * scale * scale); // Metres squared, including the coarse kernel floor.
    const float delta = max(distance - moments.x, 0);
    const float probability = variance / (variance + delta * delta);
    return probability * probability * probability;
}
/// @brief Returns convolved E/pi and coverage separately, so missing corners retain the existing ambient response.
float3 sampleProbeCascade(float3 worldPos, float3 normal, uint cascade, out float coverage)
{
    const float scale = VVE_PROBE_SCALE(cascade);
    const float spacing = VVE_PROBE_SPACING * scale;
    const float3 origin = cascade == 0 ? VVE_IRRADIANCE_ORIGIN.xyz : (floor(VVE_IRRADIANCE_EYE / spacing) - VVE_PROBE_SIDE / 2) * spacing;
    const float3 cell = (worldPos - origin) / spacing;
    const int3 base = int3(floor(cell));
    const float3 f = frac(cell);
    const float3 remaining = float(VVE_PROBE_SIDE - 1) - cell;
    const float edge = saturate(min(min(cell.x, cell.y), cell.z) / VVE_PROBE_BLEND_CELLS) *
        saturate(min(min(remaining.x, remaining.y), remaining.z) / VVE_PROBE_BLEND_CELLS);
    float3 irradiance = float3(0);
    float weightSum = 0;
    coverage = 0;
    const float3 biased = worldPos + (normal * VVE_PROBE_NORMAL_BIAS + probeNormalizeOrZero(VVE_IRRADIANCE_EYE - worldPos) * VVE_PROBE_VIEW_BIAS) * scale;
    // Invalid corners contribute no probe weight; their share stays with the fallback, avoiding black initialization holes.
    for (uint corner = 0; corner < 8; ++corner) {
        const int3 offset = int3(corner & 1, (corner >> 1) & 1, (corner >> 2) & 1);
        const int3 grid = base + offset;
        if (any(grid < 0) || any(grid >= VVE_PROBE_SIDE)) continue;
        const int3 worldCell = int3(round(origin / spacing)) + grid;
        const uint index = VVE_PROBE_ROW(worldCell.x, worldCell.y, worldCell.z);
        const float4 c0 = probeLoad(int3(0,index,0), cascade);
        if (c0.w < .5) continue;
        const float3 weights = lerp(1-f, f, float3(offset));
        const float trilinear = weights.x * weights.y * weights.z;
        const float3 probe = origin + float3(grid) * spacing;
        // Wrapped cosine smoothly prefers probes in the surface hemisphere, with a finite floor at grazing angles.
        const float facing = .5 + .5 * dot(normal, probeNormalizeOrZero(probe - worldPos));
        const float directionWeight = max(.05, facing * facing);
        const float visibility = VVE_IRRADIANCE_VISIBILITY != 0 ? probeVisibility(index, biased - probe, cascade) : 1.0;
        const float weight = trilinear * directionWeight * visibility;
        float3 value = c0.rgb * .2820947918;
        for (uint c = 1; c < VVE_PROBE_SH_COEFFICIENTS; ++c) value += probeLoad(int3(c,index,0), cascade).rgb * (.4886025119 * normal[c-1]);
        irradiance += max(value, float3(0)) * weight;
        weightSum += weight;
        coverage += trilinear * edge;
    }
    // Missing/inactive corners retain their fallback share. Visibility only selects between surviving probes;
    // if every corner is occluded, use the fallback entirely rather than normalizing noise or producing a black hole.
    if (weightSum <= 1e-6) { coverage = 0; return float3(0); }
    return irradiance * (coverage / weightSum);
}
/// @brief Shared deferred/forward sampling: near -> coarse -> environment, with validity-weighted partition of unity.
float3 sampleProbeIrradiance(float3 worldPos, float3 normal, out float coverage)
{
    float3 irradiance = sampleProbeCascade(worldPos, normal, 0, coverage);
    if (VVE_IRRADIANCE_CASCADES > 1 && coverage < 1) {
        float coarseCoverage;
        const float3 coarse = sampleProbeCascade(worldPos, normal, 1, coarseCoverage);
        irradiance += (1 - coverage) * coarse;
        coverage += (1 - coverage) * coarseCoverage;
    }
    return irradiance;
}
