#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "../../common_defines.glsl"

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec4 fragPosition;

layout (location = 0) out vec4 outPosition;
layout (location = 1) out vec4 outNormal;
layout (location = 2) out vec4 outAlbedo;


layout(set = 2, binding = 0) uniform sampler2D shadowMap;


void main() {
    outPosition = fragPosition;
    outNormal = vec4(0.0);
    outAlbedo = fragColor;
}
