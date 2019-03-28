#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "../common_defines.glsl"
#include "../light.glsl"


layout(location = 0) in vec3 fragPosW;
layout(location = 1) in vec3 fragNormalW;
layout(location = 2) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform UniformBufferObjectPerFrame {
    perFrameData_t data;
} perFrameUBO;

layout(set = 1, binding = 0) uniform UniformBufferObjectPerObject {
    perObjectData_t data;
} perObjectUBO;

layout(set = 2, binding = 0) uniform sampler2D shadowMap[3];

layout(set = 3, binding = 0) uniform sampler2D texSampler;

void main() {

    //shadow
    int sIdx = 0;
    vec4 param = perFrameUBO.data.camera.param;
    float z = (gl_FragCoord.z / gl_FragCoord.w)/( param[1] - param[0] );
    if( z >= perFrameUBO.data.shadow[0].limits[1] ) {
        sIdx = 1;
    }
    if( z >= perFrameUBO.data.shadow[1].limits[1] ) {
        sIdx = 2;
    }
    if( z >= perFrameUBO.data.shadow[2].limits[1] ) {
        sIdx = 3;
    }
    shadowData_t s = perFrameUBO.data.shadow[sIdx];
    float shadowFactor = shadowFunc(fragPosW, s.shadowView,
                                    s.shadowProj, shadowMap[sIdx] );

    //parameters
    vec3 camPosW = perFrameUBO.data.camera.camModel[3].xyz;
    vec3 lightPosW = perFrameUBO.data.light.transform[3].xyz;
    vec3 lightDirW = normalize( perFrameUBO.data.light.transform[2].xyz );
    vec4 lightParam = perFrameUBO.data.light.param;

    vec3 ambcol  = perFrameUBO.data.light.col_ambient.xyz;
    vec3 diffcol = perFrameUBO.data.light.col_diffuse.xyz;
    vec3 speccol = perFrameUBO.data.light.col_specular.xyz;

    vec3 fragColor = texture(texSampler, (fragTexCoord + perObjectUBO.data.param.zw)*perObjectUBO.data.param.xy ).xyz;

    vec3 result = vec3(0,0,0);

    result += perFrameUBO.data.light.itype[0] == 0?
                  dirlight( camPosW,
                            lightDirW, lightParam, shadowFactor,
                            ambcol, diffcol, speccol,
                            fragPosW, fragNormalW, fragColor) : vec3(0,0,0);

    result += perFrameUBO.data.light.itype[0] == 1?
                  pointlight( camPosW,
                              lightPosW, lightParam, shadowFactor,
                              ambcol, diffcol, speccol,
                              fragPosW, fragNormalW, fragColor) : vec3(0,0,0);

    result += perFrameUBO.data.light.itype[0] == 2?
                  spotlight( camPosW,
                             lightPosW, lightDirW, lightParam, shadowFactor,
                             ambcol, diffcol, speccol,
                             fragPosW, fragNormalW, fragColor) : vec3(0,0,0);

    outColor = vec4( result, 1.0 );
}
