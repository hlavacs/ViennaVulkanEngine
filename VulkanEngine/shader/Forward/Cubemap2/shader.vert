#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_ARB_shading_language_420pack : enable

#include "../common_defines.glsl"

layout(set = 0, binding = 0) uniform UniformBufferObjectPerFrame {
    perFrameData_t data;
} perFrameUBO;

layout(set = 1, binding = 0) uniform UniformBufferObjectPerObject {
    perObjectData_t data;
} perObjectUBO;

layout (location = 0) in vec3 inPositionL;

layout(location = 0) out vec3 fragPosL;
layout(location = 1) out vec3 fragPosW;


out gl_PerVertex
{
	vec4 gl_Position;
};

void main()
{
     vec4 glp = perFrameUBO.data.camera.camProj * perFrameUBO.data.camera.camView * perObjectUBO.data.model * vec4(inPositionL, 1.0);
     gl_Position  = glp; vec4( glp.x, glp.y, glp.z, glp.z*1.000001 );
     fragPosL = inPositionL;
     fragPosW = (perObjectUBO.data.model * vec4(inPositionL, 1.0)).xyz;
}
