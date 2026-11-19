/** @file @brief Slang adapter for the pinned MIT SMAA 1x reference; HIGH, no temporal reprojection. */
[[vk::binding(0, 0)]] Texture2D<float4> encodedColor; ///< UNORM view of the encoded sRGB input.
[[vk::binding(1, 0)]] Texture2D<float4> linearColor; ///< sRGB view, decoded before linear filtering.
[[vk::binding(2, 0)]] Texture2D<float4> edges; ///< Linear RG edge data.
[[vk::binding(3, 0)]] Texture2D<float4> area; ///< Pinned 160x560 RG8 lookup.
[[vk::binding(4, 0)]] Texture2D<float4> search; ///< Pinned packed 64x16 R8 lookup.
[[vk::binding(5, 0)]] Texture2D<float4> weights; ///< Linear RGBA blend weights.
[[vk::binding(6, 0)]] SamplerState linearClamp; ///< Required reference lookup and neighbourhood filtering.
[[vk::binding(7, 0)]] SamplerState pointClamp; ///< Exact encoded luma samples.
struct SmaaSettings {
	float4 metrics; ///< Reciprocal extent followed by extent, in drawable pixels.
	uint encodeSrgb; ///< Only the final UNORM attachment needs explicit encoding.
};
[[vk::push_constant]] ConstantBuffer<SmaaSettings> settings;

// Preserve upstream arithmetic and search logic; only adapt resource/sampling syntax to Slang.
#define SMAA_CUSTOM_SL 1
#define SMAA_PRESET_HIGH 1
#define SMAA_RT_METRICS settings.metrics
#define SMAATexture2D(tex) Texture2D<float4> tex
#define SMAATexturePass2D(tex) tex
// SMAA.hlsl integration step 4 and its HLSL_4 macros require linear filtering, including SearchTex.
#define SMAASampleLevelZero(tex, coord) tex.SampleLevel(linearClamp, coord, 0)
#define SMAASampleLevelZeroPoint(tex, coord) tex.SampleLevel(pointClamp, coord, 0)
#define SMAASampleLevelZeroOffset(tex, coord, offset) tex.SampleLevel(linearClamp, coord, 0, offset)
#define SMAASample(tex, coord) tex.SampleLevel(linearClamp, coord, 0)
#define SMAASamplePoint(tex, coord) tex.SampleLevel(pointClamp, coord, 0)
#define SMAASampleOffset(tex, coord, offset) tex.SampleLevel(linearClamp, coord, 0, offset)
#define SMAA_FLATTEN [flatten]
#define SMAA_BRANCH [branch]
#include "../../../../third_party/SMAA/SMAA.hlsl"
#if SMAA_MAX_SEARCH_STEPS != 16 || SMAA_MAX_SEARCH_STEPS_DIAG != 8 || SMAA_CORNER_ROUNDING != 25
#error SMAA HIGH configuration changed
#endif

/// @brief Detect gamma-space luma boundaries using the unmodified reference function.
/// SV_Position/positive Vulkan viewport are top-left/Y-down; the reference's negative Y offsets already address north.
[shader("fragment")]
float2 smaaEdges(float4 position : SV_Position) : SV_Target {
	float2 uv = position.xy * settings.metrics.xy;
	float4 offset[3];
	SMAAEdgeDetectionVS(uv, offset);
	return SMAALumaEdgeDetectionPS(uv, offset, encodedColor);
}

/// @brief Compute spatial blend weights with zero subsample indices (SMAA 1x).
[shader("fragment")]
float4 smaaWeights(float4 position : SV_Position) : SV_Target {
	float2 uv = position.xy * settings.metrics.xy, pixel;
	float4 offset[3];
	SMAABlendingWeightCalculationVS(uv, pixel, offset);
	return SMAABlendingWeightCalculationPS(uv, pixel, offset, edges, area, search, float4(0));
}

/// @brief Encode only for UNORM presentation; sRGB attachments perform their own encoding.
float encode(float value) {
	return value <= 0.0031308 ? 12.92 * value : 1.055 * pow(value, 1.0 / 2.4) - 0.055;
}

/// @brief Blend decoded linear colour, preserving alpha and the frame's single-encoding policy.
[shader("fragment")]
float4 smaaBlend(float4 position : SV_Position) : SV_Target {
	float2 uv = position.xy * settings.metrics.xy;
	float4 offset;
	SMAANeighborhoodBlendingVS(uv, offset);
	float4 color = SMAANeighborhoodBlendingPS(uv, offset, linearColor, weights);
	if (settings.encodeSrgb != 0) color.rgb = float3(encode(color.r), encode(color.g), encode(color.b));
	return color;
}
