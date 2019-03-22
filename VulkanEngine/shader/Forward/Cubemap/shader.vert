#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_ARB_shading_language_420pack : enable

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

layout (location = 0) in vec3 inPosition;

layout (location = 0) out vec3 outUVW;

out gl_PerVertex
{
	vec4 gl_Position;
};

void main()
{
     outUVW = inPosition;
     outUVW.x *= -1.0;
     vec4 glp = UBOPerFrame.camProj * UBOPerFrame.camView * UBOPerObject.model * vec4(inPosition, 1.0);
     gl_Position  = vec4( glp.x, glp.y, glp.z, glp.z*1.000001 );
}
