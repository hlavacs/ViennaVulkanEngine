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

layout(set = 2, binding = 0) uniform sampler2D shadowMap;

layout(set = 3, binding = 0) uniform sampler2D texSampler;

void main() {

    /*int sIdx = 0;
    if( gl_Position > perFrameUBO.data.shadow[0].zLimit ) {
      sIdx = 1;
    }
    if( gl_Position > perFrameUBO.data.shadow[1].zLimit ) {
      sIdx = 2;
    }*/

    //parameters
    vec3 camPosW = perFrameUBO.data.camera.camModel[3].xyz;

    vec3 lightPosW = perFrameUBO.data.light.transform[3].xyz;
    vec3 lightDirW = normalize( perFrameUBO.data.light.transform[2].xyz );
    vec4 lightParam = perFrameUBO.data.light.param;
    float shadowFactor = shadowFunc( fragPosW, perFrameUBO.data.shadow.shadowView, perFrameUBO.data.shadow.shadowProj, shadowMap );

    vec3 ambcol  = perFrameUBO.data.light.col_ambient.xyz;
    vec3 diffcol = perFrameUBO.data.light.col_diffuse.xyz;
    vec3 speccol = perFrameUBO.data.light.col_specular.xyz;

    vec3 fragColor = texture(texSampler, fragTexCoord).xyz;

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
