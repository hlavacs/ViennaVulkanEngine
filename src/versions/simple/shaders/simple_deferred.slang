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
    const float4 homogeneous = mul(frame.inverseViewProjection, float4(position.xy / frame.viewport.xy * 2.0 - 1.0, depth, 1.0));
    const float3 worldPos = homogeneous.xyz / homogeneous.w;
    const uint flags = uint(floor(surface.emissiveFlags.a / 2.0));
    return float4(shadeSurface(worldPos, surface, flags), 1.0);
}
