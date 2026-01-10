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
// Simple post processing shader that does hdr to sdr tonemapping.

#include "base.h"

layout(location = 0) out vec4 OUT_Color;

layout(location = 1) in vec2 INOUT_TextureCoordsRendering;

vk_layout(binding = BIND_TEXTURE_POSTPROCESS_INPUT) uniform sampler2D IntermediateBuffer;

float A = 0.13;
float B = 0.50;
float C = 0.10;
float D = 0.20;
float E = 0.02;
float F = 0.30;

float W = 11.2;

float Uncharted2Tonemap(float x) {
    return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}

vec3 Uncharted2Tonemap(vec3 x) {
    return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}

void main() {
	float Exposure = exp2(CameraExposure);
	vec3 hdr = textureLod(IntermediateBuffer, INOUT_TextureCoordsRendering, 0).rgb * Exposure;

    float whiteScale = 1.0 / Uncharted2Tonemap(W);
    vec3 ldr = Uncharted2Tonemap(hdr);
    ldr = ldr * whiteScale;

    OUT_Color = vec4(ldr, 1.0);
}
