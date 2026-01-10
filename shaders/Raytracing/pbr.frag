#version 460
#extension GL_ARB_shader_bit_encoding : require
#extension GL_ARB_shading_language_include : require
#extension GL_ARB_explicit_attrib_location : require
#extension GL_ARB_separate_shader_objects : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference_uvec2 : require
#extension GL_GOOGLE_cpp_style_line_directive : require
#extension GL_ARB_shading_language_packing : require
#extension GL_EXT_ray_query : require
#define USE_HARDWARE_RT
#define USE_PATHTRACING
// Shader used for lighting calculations.
// Will output separate diffuse and specular lighting, so that they can be denoised separately.
// Diffuse lighting doesn't include the Gbuffer pixels albedo color, so that it has lower frequency
// content and can be more easily denoised. The albedo color will be applied in the apply_lighting shader.

#include "base.h"
#include "random.h"
#include "raytrace.h"
#include "lighting.h"

layout(location = 0) out vec4 OUT_Diffuse;
layout(location = 1) out vec4 OUT_Specular;

layout(location = 0) in vec2 INOUT_TextureCoords;
layout(location = 1) in vec2 INOUT_TextureCoordsRendering;

vk_layout(binding = BIND_TEXTURE_GBUFFER_DEPTH) uniform sampler2D GBufferDepth;
vk_layout(binding = BIND_TEXTURE_GBUFFER_NORMAL) uniform sampler2D GBufferNormal;
vk_layout(binding = BIND_TEXTURE_GBUFFER_ALBEDO) uniform sampler2D GBufferAlbedoTransparency;
vk_layout(binding = BIND_TEXTURE_GBUFFER_MATERIAL) uniform sampler2D GBufferMetallnessRoughness;

void main() {
	// Fetch and compute surface aspects from gbuffer.
	float depthFromBuffer = textureLod(GBufferDepth, INOUT_TextureCoordsRendering, 0).r;
	vec2 normalXY = textureLod(GBufferNormal, INOUT_TextureCoordsRendering, 0).rg;
	vec3 normal = decodeNormal(normalXY);
	vec4 albedoTransparency = textureLod(GBufferAlbedoTransparency, INOUT_TextureCoordsRendering, 0);
	vec2 metalnessRoughness = textureLod(GBufferMetallnessRoughness, INOUT_TextureCoordsRendering, 0).rg;

	vec4 projectedPosition = vec4(INOUT_TextureCoords * 2.0 - 1.0, depthFromBuffer, 1.0);
    vec4 worldPosBeforeW = projectedPosition * InvViewProjection;
	vec3 worldPosition = worldPosBeforeW.xyz / worldPosBeforeW.w;

    vec4 rngValues = GetRNGValues(gl_FragCoord);

    SurfacePoint point;
    point.Position = worldPosition;
    point.Normal = normal;
    point.Albedo = albedoTransparency;
    point.Metalness = metalnessRoughness.r;
    point.Roughness = metalnessRoughness.g;

    vec3 viewVec = normalize(CameraPosition - worldPosition); 
    #ifdef USE_PATHTRACING
        point.Albedo.rgb = vec3(1.0, 1.0, 1.0);
        ShadingResult result = ShadePointPT(point, viewVec, rngValues);
    #else
        ShadingResult result = ShadePoint(point, viewVec, rngValues);
    #endif

    OUT_Diffuse = vec4(result.Diffuse, 1.0);
    OUT_Specular = vec4(result.Specular, 1.0);
}
