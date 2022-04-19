#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_ARB_shading_language_420pack : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "../../common_defines.glsl"
#include "../../light.glsl"

layout(early_fragment_tests) in;

layout(location = 0) in vec3 fragPosW;
layout(location = 1) in vec3 fragNormalW;
layout(location = 2) in vec2 fragTexCoord;

layout(location = 0) out vec4 outPosition;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outAlbedo;

layout(set = 0, binding = 0) uniform cameraUBO_t {
    cameraData_t data;
} cameraUBO;

layout(set = 1, binding = 0) uniform objectUBO_t {
    objectData_t data;
} objectUBO;

layout(set = 2, binding = 0) uniform sampler2D texSamplerArray[RESOURCEARRAYLENGTH];

void main() {
    vec4 texParam   = objectUBO.data.param;
    vec2 texCoord   = (fragTexCoord + texParam.zw)*texParam.xy;
    ivec4 iparam    = objectUBO.data.iparam;
    uint resIdx     = iparam.x % RESOURCEARRAYLENGTH;
    vec3 fragColor = texture(texSamplerArray[resIdx], texCoord).xyz;

    outAlbedo = vec4(fragColor, 1.0);
    outNormal = vec4(fragNormalW, 1.0);
    outPosition = vec4(fragPosW, 1.0);
}
