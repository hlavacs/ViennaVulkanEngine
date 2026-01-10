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
// Apply the final lighting from the temporary buffers to the screen.
// Diffuse lighting will be multiplied with gbuffer texture, specular lighting has texture contribution already baked in.

#include "base.h"

layout(location = 0) out vec4 OUT_Color;
layout(location = 0) in vec2 INOUT_TextureCoords;
layout(location = 1) in vec2 INOUT_GBufferTextureCoords;

vk_layout(binding = BIND_TEXTURE_GBUFFER_ALBEDO) uniform sampler2D GBufferAlbedoTransparency;

vk_layout(binding = BIND_TEXTURE_LIGHTING_DIFFUSE_SOURCE) uniform sampler2D LightingDiffuse;
vk_layout(binding = BIND_TEXTURE_LIGHTING_SPECULAR_SOURCE) uniform sampler2D LightingSpecular;

void main() {
    vec2 texCoord = INOUT_GBufferTextureCoords; 

	vec4 albedoTransparency = textureLod(GBufferAlbedoTransparency, texCoord, 0);

	vec3 lightingDiffuse = textureLod(LightingDiffuse, texCoord, 0).rgb;
	vec3 lightingSpecular = textureLod(LightingSpecular, texCoord, 0).rgb;

    OUT_Color = vec4((albedoTransparency.rgb / M_PI) * lightingDiffuse + lightingSpecular, 1.0);
}
