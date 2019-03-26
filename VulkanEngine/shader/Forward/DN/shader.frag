#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "../common_defines.glsl"
#include "../light.glsl"

layout(location = 0) in vec3 fragPosW;
layout(location = 1) in vec3 fragNormalW;
layout(location = 2) in vec3 fragTangentW;
layout(location = 3) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform UniformBufferObjectPerFrame {
    perFrameData_t data;
} perFrameUBO;

layout(set = 2, binding = 0) uniform sampler2D shadowMap[3];

layout(set = 3, binding = 0) uniform sampler2D texSampler;
layout(set = 3, binding = 1) uniform sampler2D normalSampler;


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
    float shadowFactor = shadowFunc(  fragPosW, s.shadowView,
                                      s.shadowProj, shadowMap[sIdx] );

    //TBN matrix
    vec3 N        = normalize( fragNormalW );
    vec3 T        = normalize( fragTangentW );
    T             = normalize( T - dot(T, N)*N );
    vec3 B        = normalize( cross( T, N ) );
    mat3 TBN      = mat3(T,B,N);
    vec3 mapnorm  = normalize( texture(normalSampler, fragTexCoord).xyz*2.0 - 1.0 );
    vec3 normal   = normalize( TBN * mapnorm );

    //parameters
    vec3 camPos   = perFrameUBO.data.camera.camModel[3].xyz;

    vec3 lightPos = perFrameUBO.data.light.transform[3].xyz;
    vec3 lightDir = normalize( perFrameUBO.data.light.transform[2].xyz );
    float nfac = dot( fragNormalW, -lightDir)<0? 0.5:1;
    vec4 lightParam = perFrameUBO.data.light.param;

    vec3 ambcol  = perFrameUBO.data.light.col_ambient.xyz;
    vec3 diffcol = perFrameUBO.data.light.col_diffuse.xyz;
    vec3 speccol = perFrameUBO.data.light.col_specular.xyz;

    vec3 fragColor = texture(texSampler, fragTexCoord).xyz;

    vec3 result = ambcol * fragColor;

    result += perFrameUBO.data.light.itype[0] == 0?
                  dirlight( camPos,
                            lightDir, lightParam, shadowFactor,
                            vec3(0,0,0), diffcol, speccol,
                            fragPosW, normal, fragColor) * nfac : vec3(0,0,0);

    result += perFrameUBO.data.light.itype[0] == 1?
                  pointlight( camPos,
                              lightPos, lightParam, shadowFactor,
                              vec3(0,0,0), diffcol, speccol,
                              fragPosW, normal, fragColor) * nfac : vec3(0,0,0);

    result += perFrameUBO.data.light.itype[0] == 2?
                  spotlight( camPos,
                             lightPos, lightDir, lightParam, shadowFactor,
                             vec3(0,0,0), diffcol, speccol,
                             fragPosW, normal, fragColor) * nfac : vec3(0,0,0);

    outColor = vec4( result, 1.0 );
}
