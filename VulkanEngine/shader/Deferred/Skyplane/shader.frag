#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "../common_defines.glsl"
#include "../light.glsl"

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec4 fragPosW;

layout (location = 0) out vec4 outColor;
layout (location = 1) out vec4 outPosition;
layout (location = 2) out vec4 outNormal;
layout (location = 3) out vec4 outAlbedo;

layout(set = 0, binding = 0) uniform cameraUBO_t {
    cameraData_t data;
} cameraUBO;

layout(set = 3, binding = 0) uniform objectUBO_t {
    objectData_t data;
} objectUBO;

layout(set = 4, binding = 0) uniform sampler2D texSampler;

void main() {

    vec3 fragColor = texture(texSampler, (fragTexCoord + objectUBO.data.param.zw)*objectUBO.data.param.xy).xyz;
	outPosition = fragPosW;
	outNormal = vec4(0.0);
	outAlbedo = vec4(0.0);
	outColor = vec4( fragColor, 1.0 );
}
