#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable
#extension GL_GOOGLE_include_directive : enable

#define RESOURCEARRAYLENGTH 16

#include "../common_defines.glsl"

layout(set = 3, binding = 0) uniform objectUBO_t {
    objectData_t data;
} objectUBO;

layout (set = 4, binding = 0) uniform samplerCube samplerCubeMapArray[RESOURCEARRAYLENGTH];

layout (location = 0) in vec3 inUVW;

layout (location = 0) out vec4 outFragColor;

void main()
{
	ivec4 iparam    = objectUBO.data.iparam;
	uint resIdx     = iparam.x % RESOURCEARRAYLENGTH;

	outFragColor = texture( samplerCubeMapArray[resIdx], inUVW);
}
