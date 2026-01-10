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
// Shared vertex shader for fullscreen render passes.

#include "base.h"

out gl_PerVertex {   
	vec4 gl_Position;
};

layout(location = 0) out vec2 INOUT_TextureCoords;
layout(location = 1) out vec2 INOUT_TextureCoordsRendering;
layout(location = 2) out vec2 INOUT_TextureCoordsRenderingOld;

void main() {
    // Compute vertex coords from current vertex id.
	float x = -1.0 + float((gl_VertexID & 1) << 2);
    float y = -1.0 + float((gl_VertexID & 2) << 1);
    gl_Position = vec4(x, y, 0.9999, 1);

    // Compute texture coordinates.
    INOUT_TextureCoords = vec2((x + 1.0) * 0.5, (y + 1.0) * 0.5);

    // Compute texture coordinates used for scale independent rendering (used for DRS).
    INOUT_TextureCoordsRendering = INOUT_TextureCoords * RenderingScale;
    INOUT_TextureCoordsRenderingOld = INOUT_TextureCoords * RenderingScaleOld;
}
