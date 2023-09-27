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
layout(location = 2) in vec3 fragTangentW;
layout(location = 3) in vec2 fragTexCoord;


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
layout(set = 2, binding = 1) uniform sampler2D normalSamplerArray[RESOURCEARRAYLENGTH];


void main() {
    vec4 texParam   = objectUBO.data.param;
    vec2 texCoord   = (fragTexCoord + texParam.zw)*texParam.xy;
    ivec4 iparam    = objectUBO.data.iparam;
    uint resIdx     = iparam.x % RESOURCEARRAYLENGTH;

    //TBN matrix
    vec3 N        = normalize(fragNormalW);
    vec3 T        = normalize(fragTangentW);
    T             = normalize(T - dot(T, N)*N);
    vec3 B        = normalize(cross(T, N));
    mat3 TBN      = mat3(T, B, N);
    vec3 mapnorm  = normalize(texture(normalSamplerArray[resIdx], texCoord).xyz*2.0 - 1.0);
    vec3 normalW  = normalize(TBN * mapnorm);

    vec3 fragColor = texture(texSamplerArray[resIdx], texCoord).xyz;

    outPosition = vec4(fragPosW, 1.0);
    outNormal = vec4(normalW, 1.0);
    outAlbedo = vec4(fragColor, 1.0);
}
