/// @file
/// @brief Ray tracing stages of the standalone radiance cascades renderer.
///
/// Functional objects:
/// - RcFrame, RcMaterial, RcPayload, RcIntervalPayload: push constants, one material row, and the ray payloads.
/// - primaryRaygen, primaryMiss, primaryClosestHit: one camera ray per pixel; a hit returns the material colour or,
///   in the voxel view, the albedo of the voxel at the hit point; a miss returns the background colour. The atlas
///   view shows one layer of a cascade atlas instead of tracing.
/// - octahedralDirection: direction of an octahedral map coordinate; RadianceCascadesRenderer::octahedralDirection mirrors it.
/// - cascadeRaygen, cascadeMiss, cascadeClosestHit: one ray per atlas texel over the cascade's interval; a hit stores the
///   voxel radiance at the hit point with transparency 0, a miss stores no radiance with transparency 1.
///   They use miss index 1 and hit group offset 1.
#include "simple_shared.h"

/// @brief Camera, background and voxel volume of one frame; C++ fills the same layout as push constants.
struct RcFrame {
	float4 origin;     ///< Camera position.
	float4 right;      ///< Camera right, scaled by tan(fov / 2) * aspect.
	float4 up;         ///< Camera up, scaled by tan(fov / 2).
	float4 forward;    ///< Normalized view direction.
	float4 background; ///< Linear colour of rays that hit nothing.
	float4 volume;     ///< Voxel volume origin (xyz) and voxel size (w).
	float4 grid;       ///< Voxels per axis (xyz) and the RadianceCascadesView (w): 0 shaded, 1 voxel albedo, 2 cascade atlas.
	float4 stage;      ///< Cascade trace: probe spacing, interval start, interval end, direction side. Atlas view: layer (x).
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

/// @brief Result of one cascade interval ray.
struct RcIntervalPayload {
	float4 interval; ///< Radiance gathered over the interval (rgb) and transparency (a): 1 when nothing was hit.
};

[[vk::binding(VVE_RC_SCENE_BINDING, 0)]]
RaytracingAccelerationStructure rcScene; ///< Top-level structure over the scene instances.
[[vk::binding(VVE_RC_OUTPUT_BINDING, 0)]] [[vk::image_format("rgba16f")]]
RWTexture2D<float4> rcOutput; ///< Linear HDR image that is blitted to the swapchain.
[[vk::binding(VVE_RC_MATERIAL_BINDING, 0)]]
StructuredBuffer<RcMaterial> rcMaterials; ///< Material rows in RenderScene order, plus a white fallback.
[[vk::binding(VVE_RC_VOXEL_ALBEDO_BINDING, 0)]]
Texture2DArray<float4> rcVoxelAlbedo; ///< Average albedo per voxel, alpha = occupancy; layer = z.
[[vk::binding(VVE_RC_VOXEL_EMISSIVE_BINDING, 0)]]
Texture2DArray<float4> rcVoxelEmissive; ///< Average emitted radiance per voxel; layer = z.
[[vk::binding(VVE_RC_VOXEL_NORMAL_BINDING, 0)]]
Texture2DArray<float4> rcVoxelNormal; ///< Average face normal per voxel; layer = z.
[[vk::binding(VVE_RC_ATLAS_BINDING, VVE_RC_CASCADE_SET)]] [[vk::image_format("rgba16f")]]
RWTexture2DArray<float4> rcAtlas; ///< Atlas of the bound cascade: a side x side tile per probe in x and y, layer = probe z.
[[vk::push_constant]]
RcFrame rcFrame; ///< Frame constants.

/// @brief Voxel that contains a world position, clamped into the grid.
int4 voxelAt(float3 position) {
	const int3 cell = int3(floor((position - rcFrame.volume.xyz) / rcFrame.volume.w));
	return int4(clamp(cell, int3(0, 0, 0), int3(rcFrame.grid.xyz) - 1), 0);
}

/// @brief Unit direction of an octahedral map coordinate in [-1, 1]^2: the centre is +y, the corners are -y.
float3 octahedralDirection(float2 e) {
	float3 v = float3(e.x, 1.0 - abs(e.x) - abs(e.y), e.y);
	if (v.y < 0.0) {
		const float2 signs = float2(v.x >= 0.0 ? 1.0 : -1.0, v.z >= 0.0 ? 1.0 : -1.0);
		v.xz = (1.0 - abs(v.zx)) * signs;
	}
	return normalize(v);
}

/// @brief Traces the camera ray through the centre of each pixel; row 0 is the top of the image.
/// The atlas view instead shows one layer of the bound cascade's atlas stretched over the image: texels whose
/// interval hit something show their radiance, texels that saw through show the background colour.
[shader("raygeneration")]
void primaryRaygen() {
	const uint2 pixel = DispatchRaysIndex().xy;
	const float2 uv = (float2(pixel) + 0.5) / float2(DispatchRaysDimensions().xy);
	if (rcFrame.grid.w == 2.0) {
		uint width, height, layers;
		rcAtlas.GetDimensions(width, height, layers);
		const float4 interval = rcAtlas[uint3(uint2(uv * float2(width, height)), uint(rcFrame.stage.x))];
		rcOutput[pixel] = float4(interval.a < 0.5 ? interval.rgb : rcFrame.background.rgb, 1.0);
		return;
	}
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

/// @brief Returns the base colour of the hit instance's material, or the voxel albedo in the voxel view.
/// The voxelizer marks every voxel a triangle touches, so the unmodified hit point always lies in a marked voxel;
/// magenta therefore reveals a voxelization bug.
[shader("closesthit")]
void primaryClosestHit(inout RcPayload payload, in BuiltInTriangleIntersectionAttributes attributes) {
	if (rcFrame.grid.w == 1.0) {
		const float4 albedo = rcVoxelAlbedo.Load(voxelAt(WorldRayOrigin() + WorldRayDirection() * RayTCurrent()));
		payload.radiance = albedo.a > 0.5 ? albedo.rgb : float3(1.0, 0.0, 1.0);
		return;
	}
	payload.radiance = rcMaterials[InstanceID()].baseColor.rgb;
}

/// @brief Traces one interval ray per atlas texel of the bound cascade. Texel (x, y, layer) belongs to probe
/// (x / side, y / side, layer), which sits at volume origin + probe * spacing, and to the octahedral direction of the
/// texel centre inside the probe's tile; the ray covers [interval start, interval end].
[shader("raygeneration")]
void cascadeRaygen() {
	const uint3 texel = DispatchRaysIndex().xyz;
	const uint side = uint(rcFrame.stage.w);
	const float3 probe = float3(float2(texel.xy / side), float(texel.z));
	RayDesc ray;
	ray.Origin = rcFrame.volume.xyz + probe * rcFrame.stage.x;
	ray.Direction = octahedralDirection((float2(texel.xy % side) + 0.5) / float(side) * 2.0 - 1.0);
	ray.TMin = rcFrame.stage.y;
	ray.TMax = rcFrame.stage.z;
	RcIntervalPayload payload;
	payload.interval = float4(0.0, 0.0, 0.0, 1.0);
	TraceRay(rcScene, RAY_FLAG_NONE, 0xFF, 1, 0, 1, ray, payload);
	rcAtlas[texel] = payload.interval;
}

/// @brief An interval that hits nothing adds no radiance and lets everything behind it through.
[shader("miss")]
void cascadeMiss(inout RcIntervalPayload payload) {
	payload.interval = float4(0.0, 0.0, 0.0, 1.0);
}

/// @brief A hit ends the interval: the radiance of the voxel at the hit point, nothing behind it.
/// The voxel radiance is currently the voxel's emission; the inject stage will add the direct light.
[shader("closesthit")]
void cascadeClosestHit(inout RcIntervalPayload payload, in BuiltInTriangleIntersectionAttributes attributes) {
	const float3 hit = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
	payload.interval = float4(rcVoxelEmissive.Load(voxelAt(hit)).rgb, 0.0);
}
