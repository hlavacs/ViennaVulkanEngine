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
// Vertex shader to render meshes to gbuffer.

#include "base.h"

layout(location = 0) in vec3 IN_Position;
layout(location = 1) in vec3 IN_Normal;
layout(location = 2) in vec2 IN_TexCoord;

out gl_PerVertex {   
	vec4 gl_Position;
};

layout(location = 0) out vec3 INOUT_Normal;
layout(location = 1) out vec2 INOUT_TexCoord;
layout(location = 2) out vec4 INOUT_ViewOld;
layout(location = 3) out vec4 INOUT_ViewNew;
layout(location = 4) out uint INOUT_MaterialIndex;

void main() {
    DrawData drawData = GetDrawData(DrawIndex);
    vec4 worldPosition = drawData.World * vec4(IN_Position, 1);
    INOUT_Normal = (drawData.WorldInvTrans * vec4(IN_Normal, 0)).xyz;
    INOUT_TexCoord = IN_TexCoord;
    INOUT_ViewOld = worldPosition * ViewProjectionOld;
    INOUT_ViewNew = worldPosition * ViewProjection;
    INOUT_MaterialIndex = drawData.MaterialIndex;

    gl_Position = INOUT_ViewNew;
}

