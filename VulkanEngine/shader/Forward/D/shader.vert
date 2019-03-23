#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "../common_defines.glsl"

layout(set = 0, binding = 0) uniform UniformBufferObjectPerFrame {
    mat4 camModel;
    mat4 camView;
    mat4 camProj;
    mat4 shadowView;
    mat4 shadowProj;
    light_t light1;
} UBOPerFrame;

layout(set = 1, binding = 0) uniform UniformBufferObjectPerObject {
    mat4 model;
    mat4 modelInvTrans;
    vec4 color;
} UBOPerObject;

layout(location = 0) in vec3 inPositionL;
layout(location = 1) in vec3 inNormalL;
layout(location = 2) in vec3 inTangentL;
layout(location = 3) in vec2 inTexCoord;

layout(location = 0) out vec3 fragPosW;
layout(location = 1) out vec3 fragNormalW;
layout(location = 2) out vec2 fragTexCoord;

out gl_PerVertex {
    vec4 gl_Position;
};


void main() {
    gl_Position = UBOPerFrame.camProj * UBOPerFrame.camView * UBOPerObject.model * vec4(inPositionL, 1.0);
    fragPosW       = (UBOPerObject.model         * vec4(inPositionL, 1.0)).xyz;
    fragNormalW    = (UBOPerObject.modelInvTrans * vec4( inNormalL, 1.0 )).xyz;
    fragTexCoord  = inTexCoord;
}
