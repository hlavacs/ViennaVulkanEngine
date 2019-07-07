#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (set = 4, binding = 0) uniform samplerCube samplerCubeMap;

layout (location = 0) in vec3 inUVW;
layout (location = 1) in vec4 inWorldPos;


layout (location = 0) out vec4 outColor;
layout (location = 1) out vec4 outPosition;
layout (location = 2) out vec4 outNormal;
layout (location = 3) out vec4 outAlbedo;



void main()
{
    outPosition  = inWorldPos;
	outNormal = vec4(0.0);
	outAlbedo = vec4(0.0);
	outColor = texture( samplerCubeMap, inUVW);
}
