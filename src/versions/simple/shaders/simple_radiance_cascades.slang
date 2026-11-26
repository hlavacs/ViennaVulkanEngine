/// @file
/// @brief Ray tracing stages of the standalone radiance cascades renderer.
///
/// Functional objects:
/// - RcFrame, RcMaterial, RcPayload, RcIntervalPayload: push constants, one material row, and the ray payloads.
/// - primaryRaygen, primaryMiss, primaryClosestHit, cascadeIrradiance: one camera ray per pixel; a hit returns the
///   emission plus albedo times (direct light + irradiance from merged cascade 0 / pi), the material base colour or,
///   in the voxel view, the albedo of the voxel at the hit point; a miss returns the background colour. The atlas
///   view shows one layer of a cascade atlas instead of tracing.
/// - octahedralDirection: direction of an octahedral map coordinate; RadianceCascadesRenderer::octahedralDirection mirrors it.
/// - cascadeRaygen, cascadeMiss, cascadeClosestHit: one ray per atlas texel over the cascade's interval; a hit stores the
///   voxel radiance at the hit point with transparency 0, a miss stores no radiance with transparency 1. The last
///   cascade's rays reach the sky: its misses store the background colour with transparency 1.
///   They use miss index 1 and hit group offset 1.
/// - RcShadowPayload, RcLight, directLight, shadowMiss: direct light at a point from every light a shadow ray reaches,
///   as the forward renderer models it (quadratic range falloff, smoothstep spot cone, no ambient); shadow rays use
///   miss index 2, accept the first hit and skip hit shaders.
/// - injectRaygen: one invocation per voxel; writes the voxel radiance volume the cascade hits read.
/// - cascadeMergeRaygen: one invocation per texel of a cascade, traces no ray; merges the already merged next cascade
///   into it in place (paper equation 11): L = L_near + T_near * L_far, T = T_near * T_far, where far is the trilinear
///   interpolation over the 8 surrounding next-cascade probes of the average of the texel's child directions.
#include "simple_shared.h"

/// @brief Camera, background and voxel volume of one frame; C++ fills the same layout as push constants.
struct RcFrame {
	float4 origin;     ///< Camera position.
	float4 right;      ///< Camera right, scaled by tan(fov / 2) * aspect.
	float4 up;         ///< Camera up, scaled by tan(fov / 2).
	float4 forward;    ///< Normalized view direction.
	float4 background; ///< Linear colour of rays that hit nothing (rgb); w is 1 while tracing the last cascade, whose misses see it as sky.
	float4 volume;     ///< Voxel volume origin (xyz) and voxel size (w).
	float4 grid;       ///< Voxels per axis (xyz) and the RadianceCascadesView (w): 0 shaded, 1 voxel albedo, 2 cascade atlas, 3 base colour.
	float4 stage;      ///< Cascade trace: probe spacing, interval start, interval end, direction side. Merge: near and far
	                   ///< probe spacing, near and far direction side. Resolve: atlas view layer, cascade 0 probe spacing and direction side.
};

/// @brief One row of the material buffer; the instance custom index selects it.
struct RcMaterial {
	float4 baseColor; ///< Linear base colour factor.
	float4 emissive;  ///< Linear emitted radiance factor.
};

/// @brief Result carried back from the hit and miss stages.
struct RcPayload {
	float3 radiance; ///< Linear colour seen along the ray; for a lit hit, its emission.
	float3 position; ///< Lit hit: world position.
	float3 normal;   ///< Lit hit: unit normal facing the ray.
	float3 albedo;   ///< Lit hit: material base colour.
	float lit;       ///< 1 when primaryRaygen must add the reflected light of the hit, else 0.
};

/// @brief Result of one cascade interval ray.
struct RcIntervalPayload {
	float4 interval; ///< Radiance gathered over the interval (rgb) and transparency (a): 1 when nothing was hit.
};

/// @brief Result of one shadow ray.
struct RcShadowPayload {
	float visible; ///< 1 when the ray reached the light, else 0.
};

/// @brief One light row, matching the forward renderer's light model.
struct RcLight {
	float4 position;  ///< World position (xyz) and type (w): 0 directional, 1 point, 2 spot.
	float4 direction; ///< Direction from the light towards the scene (xyz) and range (w; point and spot).
	float4 radiance;  ///< Colour times intensity (rgb).
	float4 cone;      ///< Spot cone: cosine of the inner (x) and outer (y) angle.
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
[[vk::binding(VVE_RC_LIGHT_BINDING, 0)]]
StructuredBuffer<RcLight> rcLights; ///< Row 0: light count in position.x; rows 1 .. count: the lights.
[[vk::binding(VVE_RC_RADIANCE_BINDING, 0)]] [[vk::image_format("rgba16f")]]
RWTexture2DArray<float4> rcVoxelRadiance; ///< Radiance leaving each voxel: emission plus albedo times direct light; layer = z.
[[vk::binding(VVE_RC_ATLAS_BINDING, VVE_RC_FAR_CASCADE_SET)]] [[vk::image_format("rgba16f")]]
RWTexture2DArray<float4> rcFarAtlas; ///< Atlas of the next cascade, already merged; read by the merge.
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
	payload.lit = 0.0;
	TraceRay(rcScene, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);
	float3 radiance = payload.radiance;
	if (payload.lit > 0.5) {
		// Lambertian: direct light (shadow rays start a millimetre off the surface) plus the cascade irradiance / pi.
		const float3 direct = directLight(payload.position + payload.normal * 0.001, payload.normal);
		radiance += payload.albedo * (direct + cascadeIrradiance(payload.position, payload.normal) / 3.14159265);
	}
	rcOutput[pixel] = float4(radiance, 1.0);
}

/// @brief Rays that leave the scene see the background colour.
[shader("miss")]
void primaryMiss(inout RcPayload payload) {
	payload.radiance = rcFrame.background.rgb;
}

/// @brief Irradiance at a surface point from the bound (merged cascade 0) atlas: the trilinear blend of the 8
/// surrounding probes, each summing L(d) * max(0, n . d) over its directions with equal solid angles 4 pi / N.
/// Probes behind the surface (on the far side of its tangent plane) are left out and the remaining weights
/// renormalized, so a floor does not average in probes below it; when all are behind, all are used.
float3 cascadeIrradiance(float3 position, float3 normal) {
	const float spacing = rcFrame.stage.y;
	const uint side = uint(rcFrame.stage.z);
	uint width, height, layers;
	rcAtlas.GetDimensions(width, height, layers);
	const int3 count = int3(int(width / side), int(height / side), int(layers));
	const float3 cell = (position - rcFrame.volume.xyz) / spacing;
	const int3 base = int3(floor(cell));
	const float3 fraction = cell - float3(base);
	float3 front = float3(0.0, 0.0, 0.0), all = float3(0.0, 0.0, 0.0);
	float frontWeight = 0.0, allWeight = 0.0;
	for (uint corner = 0; corner < 8; ++corner) {
		const int3 offset = int3(int(corner & 1), int((corner >> 1) & 1), int((corner >> 2) & 1));
		const float3 weights = float3(offset) * fraction + float3(1 - offset) * (1.0 - fraction);
		const float weight = weights.x * weights.y * weights.z;
		if (weight == 0.0) { continue; }
		const uint3 probe = uint3(clamp(base + offset, int3(0, 0, 0), count - 1));
		float3 irradiance = float3(0.0, 0.0, 0.0);
		for (uint y = 0; y < side; ++y) {
			for (uint x = 0; x < side; ++x) {
				const float cosine = dot(normal, octahedralDirection((float2(x, y) + 0.5) / float(side) * 2.0 - 1.0));
				if (cosine > 0.0) { irradiance += cosine * rcAtlas[uint3(probe.xy * side + uint2(x, y), probe.z)].rgb; }
			}
		}
		all += weight * irradiance;
		allWeight += weight;
		if (dot(rcFrame.volume.xyz + float3(probe) * spacing - position, normal) >= 0.0) {
			front += weight * irradiance;
			frontWeight += weight;
		}
	}
	const float3 blended = frontWeight > 0.0 ? front / frontWeight : all / max(allWeight, 1e-6);
	return blended * (4.0 * 3.14159265 / float(side * side));
}

/// @brief Shaded view: returns the surface (emission as radiance, hit point, normal, albedo) for primaryRaygen to
/// light; the normal is the voxel's average face normal turned towards the ray (or the reversed ray where the normals
/// cancel). Base colour view: the material base colour. Voxel view: the albedo of the voxel at the hit point; the
/// voxelizer marks every voxel a triangle touches, so the unmodified hit point always lies in a marked voxel and
/// magenta reveals a voxelization bug.
[shader("closesthit")]
void primaryClosestHit(inout RcPayload payload, in BuiltInTriangleIntersectionAttributes attributes) {
	const float3 hit = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
	const RcMaterial material = rcMaterials[InstanceID()];
	if (rcFrame.grid.w == 1.0) {
		const float4 albedo = rcVoxelAlbedo.Load(voxelAt(hit));
		payload.radiance = albedo.a > 0.5 ? albedo.rgb : float3(1.0, 0.0, 1.0);
		return;
	}
	if (rcFrame.grid.w == 3.0) {
		payload.radiance = material.baseColor.rgb;
		return;
	}
	float3 normal = rcVoxelNormal.Load(voxelAt(hit)).xyz;
	if (dot(normal, normal) < 0.5) { normal = -WorldRayDirection(); }
	else if (dot(normal, WorldRayDirection()) > 0.0) { normal = -normal; }
	payload.radiance = material.emissive.rgb;
	payload.position = hit;
	payload.normal = normal;
	payload.albedo = material.baseColor.rgb;
	payload.lit = 1.0;
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

/// @brief An interval that hits nothing lets everything behind it through; only the last cascade's rays see the sky.
[shader("miss")]
void cascadeMiss(inout RcIntervalPayload payload) {
	payload.interval = float4(rcFrame.background.rgb * rcFrame.background.w, 1.0);
}

/// @brief A hit ends the interval: the radiance of the voxel at the hit point, nothing behind it.
/// The voxel radiance is written by the inject stage of the same frame.
[shader("closesthit")]
void cascadeClosestHit(inout RcIntervalPayload payload, in BuiltInTriangleIntersectionAttributes attributes) {
	const float3 hit = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
	payload.interval = float4(rcVoxelRadiance[uint3(voxelAt(hit).xyz)].rgb, 0.0);
}

/// @brief Merges the next cascade (far) into the bound one (near) in place. Near texel (x, y, layer) is probe
/// (x / side, y / side, layer) and direction (x, y) % side; its child directions in a far tile are the
/// scale x scale texels starting at direction * scale, with scale = far side / side. Far probes outside the far grid
/// are clamped to its border.
[shader("raygeneration")]
void cascadeMergeRaygen() {
	const uint3 texel = DispatchRaysIndex().xyz;
	const uint side = uint(rcFrame.stage.z);
	const uint farSide = uint(rcFrame.stage.w);
	const uint scale = farSide / side;
	uint farWidth, farHeight, farLayers;
	rcFarAtlas.GetDimensions(farWidth, farHeight, farLayers);
	const int3 farCount = int3(int(farWidth / farSide), int(farHeight / farSide), int(farLayers));
	const uint2 children = (texel.xy % side) * scale;

	// Probe position in far probe units, relative to the volume origin.
	const float3 position = float3(float2(texel.xy / side), float(texel.z)) * rcFrame.stage.x / rcFrame.stage.y;
	const int3 base = int3(floor(position));
	const float3 fraction = position - float3(base);
	float4 far = float4(0.0, 0.0, 0.0, 0.0);
	for (uint corner = 0; corner < 8; ++corner) {
		const int3 offset = int3(int(corner & 1), int((corner >> 1) & 1), int((corner >> 2) & 1));
		const float3 weights = float3(offset) * fraction + float3(1 - offset) * (1.0 - fraction);
		const float weight = weights.x * weights.y * weights.z;
		if (weight == 0.0) { continue; }
		const uint3 probe = uint3(clamp(base + offset, int3(0, 0, 0), farCount - 1));
		for (uint k = 0; k < scale; ++k) {
			for (uint l = 0; l < scale; ++l) {
				far += weight * rcFarAtlas[uint3(probe.xy * farSide + children + uint2(k, l), probe.z)];
			}
		}
	}
	far /= float(scale * scale);
	const float4 near = rcAtlas[texel];
	rcAtlas[texel] = float4(near.rgb + near.a * far.rgb, near.a * far.a);
}

/// @brief Direct light at a point with the given normal: for every light whose shadow ray from the point is
/// unoccluded, colour * intensity * max(0, n . l) * falloff. Point and spot lights fade as (1 - d / range)^2,
/// spot lights also with smoothstep(cos outer, cos inner, cos angle); directional lights are at the sky distance.
float3 directLight(float3 position, float3 normal) {
	float3 light = float3(0.0, 0.0, 0.0);
	const uint count = uint(rcLights[0].position.x);
	for (uint index = 1; index <= count; ++index) {
		const RcLight source = rcLights[index];
		float3 toLight = -normalize(source.direction.xyz);
		float distance = 10000.0; // Matches the primary rays' TMax.
		float falloff = 1.0;
		if (source.position.w != 0.0) {
			const float3 offset = source.position.xyz - position;
			distance = length(offset);
			toLight = offset / max(distance, 0.001);
			const float range = source.direction.w;
			const float fade = range > 0.0 ? saturate(1.0 - distance / range) : 0.0;
			falloff = fade * fade;
			if (source.position.w == 2.0) { falloff *= smoothstep(source.cone.y, source.cone.x, dot(-toLight, normalize(source.direction.xyz))); }
		}
		const float cosine = dot(normal, toLight);
		if (cosine <= 0.0 || falloff <= 0.0) { continue; }
		RayDesc ray;
		ray.Origin = position;
		ray.Direction = toLight;
		ray.TMin = 0.0;
		ray.TMax = distance;
		RcShadowPayload shadow;
		shadow.visible = 0.0;
		TraceRay(rcScene, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER, 0xFF, 0, 0, 2, ray, shadow);
		light += source.radiance.rgb * cosine * falloff * shadow.visible;
	}
	return light;
}

/// @brief A shadow ray that reaches its light.
[shader("miss")]
void shadowMiss(inout RcShadowPayload payload) {
	payload.visible = 1.0;
}

/// @brief Writes the radiance leaving each voxel: emission plus albedo times the direct light. The light is evaluated
/// one voxel out from the voxel centre along its normal, clear of the voxel's own surface. Voxels whose normals
/// cancel (two opposite faces) receive no direct light; empty voxels hold zero.
[shader("raygeneration")]
void injectRaygen() {
	const uint3 voxel = DispatchRaysIndex().xyz;
	const int4 cell = int4(int3(voxel), 0);
	const float4 albedo = rcVoxelAlbedo.Load(cell);
	if (albedo.a < 0.5) {
		rcVoxelRadiance[voxel] = float4(0.0, 0.0, 0.0, 0.0);
		return;
	}
	const float3 normal = rcVoxelNormal.Load(cell).xyz;
	const float size = rcFrame.volume.w;
	const float3 centre = rcFrame.volume.xyz + (float3(voxel) + 0.5) * size;
	const float3 direct = dot(normal, normal) > 0.5 ? directLight(centre + normal * size, normal) : float3(0.0, 0.0, 0.0);
	rcVoxelRadiance[voxel] = float4(rcVoxelEmissive.Load(cell).rgb + albedo.rgb * direct, 1.0);
}
