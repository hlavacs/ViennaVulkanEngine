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

/// @brief Fix the rounding order across lighting/AO variants; perspective division amplifies cancellation in homogeneous coordinates.
float3 surfaceWorldPosition(float2 pixel, float depth)
{
    // Explicit FMA also works with older Slang SPIR-V compilers that ignore per-variable `precise`.
    const float2 clip = fma(pixel / frame.viewport.xy, float2(2), float2(-1));
    float4 homogeneous;
    [unroll] for (uint row = 0; row < 4; ++row)
        homogeneous[row] = fma(frame.inverseViewProjection[row][0], clip.x,
            fma(frame.inverseViewProjection[row][1], clip.y,
                fma(frame.inverseViewProjection[row][2], depth, frame.inverseViewProjection[row][3])));
    return homogeneous.xyz / homogeneous.w;
}

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
    return mul(frame.view, float4(surfaceWorldPosition(pixel, depth), 1)).xyz;
}
/// @brief Sample-zero rays use pixel centres; the 2 cm bias absorbs subpixel coverage-position differences.
float3 aoSourcePosition(int2 pixel) { return aoViewPosition(float2(pixel) + 0.5, aoSourceDepth(pixel)); }
#endif

#ifdef VVE_CONTACT_AO
[[vk::binding(6, 1)]] Texture2D<float> contactAo;
/// @brief Reuses visibility only on the same surface; unmatched MSAA coverage falls back to an unoccluded sample.
float sampleContactAo(int2 pixel, float3 viewPos, float3 normal)
{
    const float z = -viewPos.z;
    const float sourceZ = -aoSourcePosition(pixel).z;
    if (aoSourceDepth(pixel) < 1 && abs(sourceZ - z) <= VVE_AO_DEPTH_RELATIVE * z &&
        dot(aoSourceNormal(pixel), normal) >= VVE_AO_NORMAL_COS) return contactAo.Load(int3(pixel, 0));
    // A different coverage surface may be represented next door; take only the closest depth match.
    const int2 offsets[4] = {int2(-1,0), int2(1,0), int2(0,-1), int2(0,1)};
    float best = VVE_AO_DEPTH_RELATIVE * z, result = 1;
    for (uint i = 0; i < 4; ++i) {
        const int2 q = pixel + offsets[i];
        if (any(q < 0) || any(q >= int2(frame.viewport.xy)) || aoSourceDepth(q) >= 1) continue;
        const float difference = abs(-aoSourcePosition(q).z - z);
        if (difference < best && dot(aoSourceNormal(q), normal) >= VVE_AO_NORMAL_COS) {
            best = difference; result = contactAo.Load(int3(q,0));
        }
    }
    return result;
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
    if (depth >= 1.0) discard; // Keep the HDR clear colour for empty samples and the later sky pass.
    const float3 worldPos = surfaceWorldPosition(position.xy, depth);
    const uint flags = uint(floor(surface.emissiveFlags.a / 2.0));
#ifdef VVE_CONTACT_AO
    const float ao = sampleContactAo(pixel, mul(frame.view, float4(worldPos,1)).xyz, normalize(surface.normalMetalness.xyz));
    return float4(shadeSurface(worldPos, surface, flags, ao), 1.0);
#else
    return float4(shadeSurface(worldPos, surface, flags), 1.0);
#endif
}
