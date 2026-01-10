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
// Shader that combines debug rendering (RT overdraw) with main buffer and draws to final presentation buffer.

#include "base.h"

layout(location = 0) out vec4 OUT_Color;

vk_layout(binding = BIND_TEXTURE_DISPLAY_SOURCE) uniform sampler2D DisplaySource;
vk_layout(binding = BIND_TEXTURE_DEBUG_OVERDRAW) uniform sampler2D DebugSource;

void main() {
	ivec2 bufferSize = textureSize(DisplaySource, 0);
    ivec2 pixelCoord = ivec2(gl_FragCoord.xy);
    pixelCoord.y = (bufferSize.y - 1) - pixelCoord.y;
    
    float debugValue = texelFetch(DebugSource, pixelCoord, 0).r;
    OUT_Color = mix(texelFetch(DisplaySource, pixelCoord, 0), vec4(1.0, 0.0, 0.0, 1.0), debugValue);
}
