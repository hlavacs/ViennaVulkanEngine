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
// Fragment shader to output gbuffer values.

#include "base.h"

layout(location = 0) out vec4 OUT_Normal;
layout(location = 1) out vec4 OUT_AlbedoTransparency;
layout(location = 2) out vec4 OUT_MetalnessRoughness;
layout(location = 3) out vec4 OUT_Motion;

layout(location = 0) in vec3 INOUT_Normal;
layout(location = 1) in vec2 INOUT_TexCoord;
layout(location = 2) in vec4 INOUT_ViewOld;
layout(location = 3) in vec4 INOUT_ViewNew;
layout(location = 4) flat in uint INOUT_MaterialIndex;

void main() {
	RendererMaterial material = GetMaterial(INOUT_MaterialIndex);
    vec4 albedo = material.AlbedoFactor;
    if(HasFeature(material, MATERIAL_FEATURE_ALBEDO_MAP)) {
    	albedo *= SampleMaterialTexture(material.AlbedoMap, INOUT_TexCoord);	
    }

    if(HasFeature(material, MATERIAL_FEATURE_OPACITY_CUTOUT)) {
    	if (albedo.a < material.AlphaCutoff) {
		    discard;
		}
    }

    float metallic = material.MetalnessFactor;
    float roughness = material.RoughnessFactor;
	if(HasFeature(material, MATERIAL_FEATURE_METALLICROUGHNESS_MAP)) {
    	vec2 metallicRoughness = SampleMaterialTexture(material.MetallicRoughnessMap, INOUT_TexCoord).rg;
    	metallic *= metallicRoughness.r;
    	roughness *= metallicRoughness.g;	
    }

	OUT_Normal = vec4(encodeNormal(INOUT_Normal), 0.0, 1.0);
	OUT_AlbedoTransparency = albedo;
	OUT_MetalnessRoughness = vec4(metallic, roughness, 0, 1.0);

	// Compute per-pixel camera motion between last and current frame and remove jitter.
    OUT_Motion = vec4((((INOUT_ViewNew.xy / INOUT_ViewNew.w) - Jitter) - ((INOUT_ViewOld.xy / INOUT_ViewOld.w) - JitterOld)) * vec2(0.5, 0.5), 0, 0);
}
