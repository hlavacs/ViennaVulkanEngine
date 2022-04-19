#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable

#include "../common_defines.glsl"

layout(location = 0) rayPayloadInEXT hitPayload prd;

void main()
{
    if (prd.depth == 0)
    prd.hitValue += vec3(0.01);
}