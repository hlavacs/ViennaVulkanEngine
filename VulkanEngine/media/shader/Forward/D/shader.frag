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

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform cameraUBO_t {
    cameraData_t data;
} cameraUBO;

layout(set = 1, binding = 0) uniform lightUBO_t {
    lightData_t data;
} lightUBO;

layout(set = 2, binding = 0) uniform sampler2D shadowMap[NUM_SHADOW_CASCADE];

layout(set = 3, binding = 0) uniform objectUBO_t {
    objectData_t data;
} objectUBO;

layout(set = 4, binding = 0) uniform sampler2D texSamplerArray[RESOURCEARRAYLENGTH];


void main() {

    //parameters
    int  lightType  = lightUBO.data.itype[0];
    vec3 camPosW    = cameraUBO.data.camModel[3].xyz;
    vec3 lightPosW  = lightUBO.data.lightModel[3].xyz;
    vec3 lightDirW  = normalize(lightUBO.data.lightModel[2].xyz);
    vec4 lightParam = lightUBO.data.param;
    vec4 texParam   = objectUBO.data.param;
    vec2 texCoord   = (fragTexCoord + texParam.zw)*texParam.xy;
    ivec4 iparam    = objectUBO.data.iparam;
    uint resIdx     = iparam.x % RESOURCEARRAYLENGTH;
    vec3 normalW    = fragNormalW;//to be consistent with DN

    //colors
    vec3 ambcol  = lightUBO.data.col_ambient.xyz;
    vec3 diffcol = lightUBO.data.col_diffuse.xyz;
    vec3 speccol = lightUBO.data.col_specular.xyz;
    vec3 fragColor = texture(texSamplerArray[resIdx], texCoord).xyz;

    vec3 result = vec3(0, 0, 0);
    int sIdx = 0;
    cameraData_t s = lightUBO.data.shadowCameras[0];
    float shadowFactor = 1.0;

    if (lightType == LIGHT_DIR) {
        sIdx = shadowIdxDirectional(cameraUBO.data.param,
        gl_FragCoord,
        lightUBO.data.shadowCameras[0].param[3],
        lightUBO.data.shadowCameras[1].param[3],
        lightUBO.data.shadowCameras[2].param[3]);

        s = lightUBO.data.shadowCameras[sIdx];
        shadowFactor = shadowFunc(fragPosW, s.camView, s.camProj, shadowMap[sIdx]);

        result +=   dirlight(lightType, camPosW,
        lightDirW, lightParam, shadowFactor,
        ambcol, diffcol, speccol,
        fragPosW, normalW, fragColor);
    }


    if (lightType == LIGHT_POINT) {

        sIdx = shadowIdxPoint(lightPosW, fragPosW);
        s = lightUBO.data.shadowCameras[sIdx];
        shadowFactor = shadowFunc(fragPosW, s.camView, s.camProj, shadowMap[sIdx]);

        result +=   pointlight(lightType, camPosW,
        lightPosW, lightParam, shadowFactor,
        ambcol, diffcol, speccol,
        fragPosW, normalW, fragColor);
    }

    if (lightType == LIGHT_SPOT) {

        shadowFactor = shadowFunc(fragPosW, s.camView, s.camProj, shadowMap[sIdx]);

        result +=  spotlight(lightType, camPosW,
        lightPosW, lightDirW, lightParam, shadowFactor,
        ambcol, diffcol, speccol,
        fragPosW, normalW, fragColor);
    }

    if (lightType == LIGHT_AMBIENT) {
        result += fragColor * ambcol;
    }

    outColor = vec4(result, 1.0);
}
