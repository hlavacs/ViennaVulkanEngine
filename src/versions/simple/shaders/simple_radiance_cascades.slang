/// @file
/// @brief Ray tracing stages of the standalone radiance cascades renderer.
///
/// Functional objects:
/// - RcFrame, RcMaterial, RcPayload: push constants, one material row, and the ray payload.
/// - primaryRaygen, primaryMiss, primaryClosestHit: one camera ray per pixel; a hit returns the material colour,
///   a miss the background colour.
#include "simple_shared.h"

/// @brief Camera and background of one frame; C++ fills the same layout as push constants.
struct RcFrame {
	float4 origin;     ///< Camera position.
	float4 right;      ///< Camera right, scaled by tan(fov / 2) * aspect.
	float4 up;         ///< Camera up, scaled by tan(fov / 2).
	float4 forward;    ///< Normalized view direction.
	float4 background; ///< Linear colour of rays that hit nothing.
};

/// @brief One row of the material buffer; the instance custom index selects it.
struct RcMaterial {
	float4 baseColor; ///< Linear base colour factor.
	float4 emissive;  ///< Linear emitted radiance factor.
};

/// @brief Result carried back from the hit and miss stages.
struct RcPayload {
	float3 radiance; ///< Linear colour seen along the ray.
};

[[vk::binding(VVE_RC_SCENE_BINDING, 0)]]
RaytracingAccelerationStructure rcScene; ///< Top-level structure over the scene instances.
[[vk::binding(VVE_RC_OUTPUT_BINDING, 0)]] [[vk::image_format("rgba16f")]]
RWTexture2D<float4> rcOutput; ///< Linear HDR image that is blitted to the swapchain.
[[vk::binding(VVE_RC_MATERIAL_BINDING, 0)]]
StructuredBuffer<RcMaterial> rcMaterials; ///< Material rows in RenderScene order, plus a white fallback.
[[vk::push_constant]]
RcFrame rcFrame; ///< Frame constants.

/// @brief Traces the camera ray through the centre of each pixel; row 0 is the top of the image.
[shader("raygeneration")]
void primaryRaygen() {
	const uint2 pixel = DispatchRaysIndex().xy;
	const float2 uv = (float2(pixel) + 0.5) / float2(DispatchRaysDimensions().xy);
	RayDesc ray;
	ray.Origin = rcFrame.origin.xyz;
	ray.Direction = normalize(rcFrame.forward.xyz + (2.0 * uv.x - 1.0) * rcFrame.right.xyz + (1.0 - 2.0 * uv.y) * rcFrame.up.xyz);
	ray.TMin = 0.001;
	ray.TMax = 10000.0;
	RcPayload payload;
	payload.radiance = float3(0.0, 0.0, 0.0);
	TraceRay(rcScene, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);
	rcOutput[pixel] = float4(payload.radiance, 1.0);
}

/// @brief Rays that leave the scene see the background colour.
[shader("miss")]
void primaryMiss(inout RcPayload payload) {
	payload.radiance = rcFrame.background.rgb;
}

/// @brief Returns the base colour of the hit instance's material.
[shader("closesthit")]
void primaryClosestHit(inout RcPayload payload, in BuiltInTriangleIntersectionAttributes attributes) {
	payload.radiance = rcMaterials[InstanceID()].baseColor.rgb;
}
