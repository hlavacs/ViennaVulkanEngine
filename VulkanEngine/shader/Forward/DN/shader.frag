#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "../common_defines.glsl"
#include "../light.glsl"

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragTangent;
layout(location = 3) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform UniformBufferObjectPerFrame {
    mat4 camModel;
    mat4 camView;
    mat4 camProj;
    mat4 shadowView;
    mat4 shadowProj;
    light_t light1;
} UBOPerFrame;

layout(set = 3, binding = 0) uniform sampler2D texSampler;
layout(set = 3, binding = 1) uniform sampler2D normalSampler;


void main() {

    vec3 N        = normalize( fragNormal );
    vec3 T        = normalize( fragTangent );
    T             = normalize( T - dot(T, N)*N );
    vec3 B        = normalize( cross( T, N ) );
    mat3 TBN      = mat3(T,B,N);
    vec3 mapnorm  = normalize( texture(normalSampler, fragTexCoord).xyz*2.0 - 1.0 );
    vec3 normal   = normalize( TBN * mapnorm );

    //parameters
    vec3 camPos = UBOPerFrame.camModel[3].xyz;

    vec3 lightPos = UBOPerFrame.light1.transform[3].xyz;
    vec3 lightDir = normalize( UBOPerFrame.light1.transform[2].xyz );
    float nfac = dot( fragNormal, -lightDir)<0? 0:1;
    vec4 lightParam = UBOPerFrame.light1.param;

    vec3 ambcol  = UBOPerFrame.light1.col_ambient.xyz;
    vec3 diffcol = UBOPerFrame.light1.col_diffuse.xyz;
    vec3 speccol = UBOPerFrame.light1.col_specular.xyz;

    vec3 fragColor = texture(texSampler, fragTexCoord).xyz;

    vec3 result = ambcol * fragColor;

    result += UBOPerFrame.light1.itype[0] == 0?
                  dirlight( camPos,
                            lightDir, lightParam,
                            vec3(0,0,0), diffcol, speccol,
                            fragPos, normal, fragColor) * nfac : vec3(0,0,0);

    result += UBOPerFrame.light1.itype[0] == 1?
                  pointlight( camPos,
                              lightPos, lightParam,
                              vec3(0,0,0), diffcol, speccol,
                              fragPos, normal, fragColor) * nfac : vec3(0,0,0);

    result += UBOPerFrame.light1.itype[0] == 2?
                  spotlight( camPos,
                             lightPos, lightDir, lightParam,
                             vec3(0,0,0), diffcol, speccol,
                             fragPos, normal, fragColor) * nfac : vec3(0,0,0);

    outColor = vec4( result, 1.0 );
}
