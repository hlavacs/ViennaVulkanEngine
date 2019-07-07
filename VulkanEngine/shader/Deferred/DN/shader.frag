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
layout(location = 1) out vec4 outPosition;
layout(location = 2) out vec4 outNormal;
layout(location = 3) out vec4 outAlbedo;

layout(set = 0, binding = 0) uniform cameraUBO_t {
    cameraData_t data;
} cameraUBO;

layout(set = 2, binding = 0) uniform sampler2D shadowMap[NUM_SHADOW_CASCADE];

layout(set = 3, binding = 0) uniform objectUBO_t {
    objectData_t data;
} objectUBO;

layout(set = 4, binding = 0) uniform sampler2D texSampler;
layout(set = 4, binding = 1) uniform sampler2D normalSampler;


void main() {

    //TBN matrix
    vec3 N        = normalize( fragNormalW );
    vec3 T        = normalize( fragTangentW );
    T             = normalize( T - dot(T, N)*N );
    vec3 B        = normalize( cross( T, N ) );
    mat3 TBN      = mat3(T,B,N);
    vec3 mapnorm  = normalize( texture(normalSampler, (fragTexCoord + objectUBO.data.param.zw)*objectUBO.data.param.xy).xyz*2.0 - 1.0 );
    vec3 normal   = normalize( TBN * mapnorm );



    vec3 fragColor = texture(texSampler, (fragTexCoord + objectUBO.data.param.zw)*objectUBO.data.param.xy).xyz;

	outPosition = vec4(fragPosW,0);
	outNormal = vec4(normal,0);	
	outAlbedo = vec4(fragColor,0);
}
