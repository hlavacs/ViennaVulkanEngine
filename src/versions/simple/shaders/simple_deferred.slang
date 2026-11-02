#include "simple_forward.slang"
/** @file simple_deferred.slang
 * @brief Fullscreen lighting of stored opaque samples. Depth and attributes are never resolved before lighting.
 * The multisample entry reads exactly SV_SampleIndex; SV_Position is its sample location under sample shading.
 */
#ifdef VVE_DEFERRED_MSAA
[[vk::binding(0, 1)]] Texture2DMS<float4> gBase;
[[vk::binding(1, 1)]] Texture2DMS<float4> gNormal;
[[vk::binding(2, 1)]] Texture2DMS<float4> gEmissive;
[[vk::binding(3, 1)]] Texture2DMS<float> gDepth;
#else
[[vk::binding(0, 1)]] Texture2D<float4> gBase;
[[vk::binding(1, 1)]] Texture2D<float4> gNormal;
[[vk::binding(2, 1)]] Texture2D<float4> gEmissive;
[[vk::binding(3, 1)]] Texture2D<float> gDepth;
#endif

#if defined(VVE_CONTACT_AO) || defined(VVE_AO)
/// @brief AO never resolves different surfaces: depth and normal always come from coverage sample zero.
float aoSourceDepth(int2 pixel)
{
#ifdef VVE_DEFERRED_MSAA
    return gDepth.Load(pixel, 0);
#else
    return gDepth.Load(int3(pixel, 0));
#endif
}
/// @brief Returns the same representative surface normal for evaluation, filtering and edge rejection.
float3 aoSourceNormal(int2 pixel)
{
#ifdef VVE_DEFERRED_MSAA
    return normalize(gNormal.Load(pixel, 0).xyz);
#else
    return normalize(gNormal.Load(int3(pixel, 0)).xyz);
#endif
}
/// @brief Reconstructs view position in metres, avoiding nonlinear D32 comparisons.
float3 aoViewPosition(float2 pixel, float depth)
{
    const float4 h = mul(frame.inverseViewProjection, float4(pixel / frame.viewport.xy * 2.0 - 1.0, depth, 1));
    return mul(frame.view, float4(h.xyz / h.w, 1)).xyz;
}
/// @brief Sample-zero rays use pixel centres; the 2 cm bias absorbs subpixel coverage-position differences.
float3 aoSourcePosition(int2 pixel) { return aoViewPosition(float2(pixel) + 0.5, aoSourceDepth(pixel)); }
#endif

#ifdef VVE_CONTACT_AO
[[vk::binding(6, 1)]] Texture2D<float> contactAo;
/// @brief Reuses visibility only on the same surface; unmatched MSAA coverage falls back to an unoccluded sample.
float sampleContactAo(int2 pixel, float3 viewPos, float3 normal)
{
    const uint shift = object.waves.x > 1 ? 1 : 0; // Fullscreen-only AO source spacing, independent of material/water flags.
    const int stride = 1 << shift;
    const int2 source = (pixel >> shift) << shift;
    const float z = -viewPos.z;
    const float sourceZ = -aoSourcePosition(source).z;
    if (aoSourceDepth(source) < 1 && abs(sourceZ - z) <= VVE_AO_DEPTH_RELATIVE * z &&
        dot(aoSourceNormal(source), normal) >= VVE_AO_NORMAL_COS) return contactAo.Load(int3(source >> shift, 0));
    // A different coverage surface may be represented next door; take only the closest depth match.
    const int2 offsets[4] = {int2(-1,0), int2(1,0), int2(0,-1), int2(0,1)};
    float best = VVE_AO_DEPTH_RELATIVE * z, result = 1;
    for (uint i = 0; i < 4; ++i) {
        const int2 q = source + offsets[i] * stride;
        if (any(q < 0) || any(q >= int2(frame.viewport.xy)) || aoSourceDepth(q) >= 1) continue;
        const float difference = abs(-aoSourcePosition(q).z - z);
        if (difference < best && dot(aoSourceNormal(q), normal) >= VVE_AO_NORMAL_COS) {
            best = difference; result = contactAo.Load(int3(q >> shift,0));
        }
    }
    return result;
}
#endif

#ifdef VVE_GI
/// @brief Shows the finest enclosing grid's nearest row, including never-captured and inactive storage.
float3 debugProbeValidity(float3 worldPos)
{
    for (uint cascade = 0; cascade < uint(frame.viewport.w); ++cascade) {
        const float spacing = VVE_PROBE_SPACING * VVE_PROBE_SCALE(cascade);
        const float3 origin = cascade == 0 ? frame.probeOrigin.xyz : (floor(worldEye() / spacing) - VVE_PROBE_SIDE / 2) * spacing;
        const float3 cell = (worldPos - origin) / spacing;
        if (any(cell < 0) || any(cell > VVE_PROBE_SIDE - 1)) continue;
        const int3 grid = int3(round(origin / spacing) + round(cell));
        const uint row = VVE_PROBE_ROW(grid.x, grid.y, grid.z);
        if (probeLoad(int3(0,row,0),cascade).w > .5) return float3(0,1,0);
        return probeLoad(int3(VVE_PROBE_SH_COEFFICIENTS,row,0),cascade).x > 0 ? float3(1,0,0) : float3(0,0,1);
    }
    return float3(0);
}
#endif

/// @brief A vertex-buffer-free triangle covers the drawable area.
[shader("vertex")]
float4 lightingVertexMain(uint vertex : SV_VulkanVertexID) : SV_Position
{
    return float4(vertex == 1 ? 3.0 : -1.0, vertex == 2 ? 3.0 : -1.0, 0.0, 1.0);
}

/// @brief Reconstructs world position from Vulkan depth (0..1) and evaluates the common lighting function.
[shader("fragment")]
float4 lightingMain(float4 position : SV_Position
#ifdef VVE_DEFERRED_MSAA
    , uint sampleIndex : SV_SampleIndex
#endif
) : SV_Target0
{
    const int2 pixel = int2(position.xy);
    GBufferOutput surface;
#ifdef VVE_DEFERRED_MSAA
    const float depth = gDepth.Load(pixel, sampleIndex);
    surface.baseRoughness = gBase.Load(pixel, sampleIndex);
    surface.normalMetalness = gNormal.Load(pixel, sampleIndex);
    surface.emissiveFlags = gEmissive.Load(pixel, sampleIndex);
#else
    const float depth = gDepth.Load(int3(pixel, 0));
    surface.baseRoughness = gBase.Load(int3(pixel, 0));
    surface.normalMetalness = gNormal.Load(int3(pixel, 0));
    surface.emissiveFlags = gEmissive.Load(int3(pixel, 0));
#endif
#ifdef VVE_GI
    const uint debugView = object.shadowMatrixIndex; // Public GiDebugView ordinal, set only for the fullscreen lighting draw.
    if (debugView != 0 && depth >= 1) return float4(debugView == 2 || debugView == 5 ? float3(1) : float3(0),1);
#endif
    if (depth >= 1.0) discard; // Keep the HDR clear colour for empty samples and the later sky pass.
    const float4 homogeneous = mul(frame.inverseViewProjection, float4(position.xy / frame.viewport.xy * 2.0 - 1.0, depth, 1.0));
    const float3 worldPos = homogeneous.xyz / homogeneous.w;
    const uint flags = uint(floor(surface.emissiveFlags.a / 2.0));
#ifdef VVE_GI
    // These outputs bypass lighting and atmosphere; ordinary mode retains the expressions below verbatim.
    if (debugView != 0) {
        const float3 normal = normalize(surface.normalMetalness.xyz);
        float ao = 1;
#ifdef VVE_CONTACT_AO
        ao = sampleContactAo(pixel, mul(frame.view,float4(worldPos,1)).xyz,normal);
#endif
        if (debugView == 1) {
            float coverage;
            const float3 irradiance = frame.probeControl.x != 0 && frame.probeControl.y > 0 && (flags & kUnlitFlag) == 0
                ? sampleProbeIrradiance(worldPos,normal,coverage) : float3(0);
            const float materialAo = surface.emissiveFlags.a - 2.0 * float(flags);
            return float4(irradiance * surface.baseRoughness.rgb * (1-surface.normalMetalness.a) *
                frame.probeControl.y * min(materialAo,ao),1);
        }
        if (debugView == 2) return float4(float3(ao),1);
        if (debugView == 3) return float4(debugProbeValidity(worldPos),1);
        if (debugView == 4) return float4(normal * .5 + .5,1);
        const float farPlane = object.opacity; // Explicit far clip avoids cancellation when decoding a long-range projection.
        return float4(float3(saturate(-mul(frame.view,float4(worldPos,1)).z / farPlane)),1);
    }
#endif
#ifdef VVE_CONTACT_AO
    const float ao = sampleContactAo(pixel, mul(frame.view, float4(worldPos,1)).xyz, normalize(surface.normalMetalness.xyz));
    return float4(shadeSurface(worldPos, surface, flags, ao), 1.0);
#else
    return float4(shadeSurface(worldPos, surface, flags), 1.0);
#endif
}
