#version 460
#extension GL_NV_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable

#include "../common_defines.glsl"
#include "../light.glsl"

layout(location = 0) rayPayloadInNV vec3 hitValue;
layout(location = 2) rayPayloadNV bool isShadowed;
hitAttributeNV vec3 attribs;

layout(set = 0, binding = 0) uniform cameraUBO_t {
    cameraData_t data;
} cameraUBO;


layout(set = 1, binding = 0) uniform lightUBO_t {
    lightData_t data;
} lightUBO;


layout(set = 3, binding = 0) uniform accelerationStructureNV topLevelAS;

layout(set = 4, binding = 0) buffer Vertices { vec4 v[]; } vertices;
layout(set = 4, binding = 1) buffer Indices { uint i[]; } indices;

layout(set = 5, binding = 0) uniform objectUBO_t {
    objectData_t data;
} objectUBO;


layout(set = 6, binding = 0) uniform sampler2D texSamplerArray[RESOURCEARRAYLENGTH];
layout(set = 6, binding = 1) uniform sampler2D normalSamplerArray[RESOURCEARRAYLENGTH];

struct Vertex
{
    vec3 pos;
    vec3 normal;
    vec3 tangent;
    vec2 texCoord;
    int  entityId;
};

// Number of vec4 values used to represent a vertex
uint vertexSize = 3;

Vertex unpackVertex(uint index)
{
    Vertex v;

    vec4 d0 = vertices.v[vertexSize * index + 0];
    vec4 d1 = vertices.v[vertexSize * index + 1];
    vec4 d2 = vertices.v[vertexSize * index + 2];

    v.pos = d0.xyz;
    v.normal = vec3(d0.w, d1.x, d1.y);
    v.tangent = vec3(d1.z, d1.w, d2.x);
    v.texCoord = vec2(d2.y, d2.z);
    v.entityId = floatBitsToInt(d2.w);
    return v;
}


void main()
{
    ivec3 ind = ivec3(indices.i[3 * gl_PrimitiveID], indices.i[3 * gl_PrimitiveID + 1], indices.i[3 * gl_PrimitiveID + 2]);

    Vertex v0 = unpackVertex(ind.x);
    Vertex v1 = unpackVertex(ind.y);
    Vertex v2 = unpackVertex(ind.z);
    const vec3 barycentrics = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);

    vec3 fragNormalL = normalize(v0.normal * barycentrics.x + v1.normal * barycentrics.y + v2.normal * barycentrics.z);
    vec3 fragPosL = normalize(v0.pos * barycentrics.x + v1.pos * barycentrics.y + v2.pos * barycentrics.z);
    vec3 fragTangentL = normalize(v0.tangent * barycentrics.x + v1.tangent * barycentrics.y + v2.tangent * barycentrics.z);

    vec3 fragPosW        = (objectUBO.data.model         * vec4(fragPosL, 1.0)).xyz;
    vec3 fragNormalW    = (objectUBO.data.modelInvTrans * vec4(fragNormalL, 1.0)).xyz;
    vec3 fragTangentW   = (objectUBO.data.modelInvTrans * vec4(fragTangentL, 0.0)).xyz;
    vec2 fragTexCoord    = (v0.texCoord * barycentrics.x + v1.texCoord * barycentrics.y + v2.texCoord * barycentrics.z);


    int  lightType  = lightUBO.data.itype[0];
    vec3 camPosW   = cameraUBO.data.camModel[3].xyz;
    vec3 lightPosW = lightUBO.data.lightModel[3].xyz;
    vec3 lightDirW = normalize(lightUBO.data.lightModel[2].xyz);
    float nfac = dot(fragNormalW, -lightDirW)<0? 0.5:1;
    vec4 lightParam = lightUBO.data.param;
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

    // colors
    vec3 ambcol  = lightUBO.data.col_ambient.xyz;
    vec3 diffcol = lightUBO.data.col_diffuse.xyz;
    vec3 speccol = lightUBO.data.col_specular.xyz;
    vec3 fragColor = texture(texSamplerArray[resIdx], texCoord).xyz;

    vec3 result = vec3(0, 0, 0);
    if (lightType == LIGHT_DIR) {
        result +=   dirlight(lightType, camPosW,
        lightDirW, lightParam, 1.0,
        ambcol, diffcol, speccol,
        fragPosW, normalW, fragColor);
    }


    if (lightType == LIGHT_POINT) {
        result +=   pointlight(lightType, camPosW,
        lightPosW, lightParam, 1.0,
        ambcol, diffcol, speccol,
        fragPosW, normalW, fragColor);
    }

    if (lightType == LIGHT_SPOT) {
        result +=  spotlight(lightType, camPosW,
        lightPosW, lightDirW, lightParam, 1.0,
        ambcol, diffcol, speccol,
        fragPosW, normalW, fragColor);
    }

    if (lightType == LIGHT_AMBIENT) {
        result += fragColor * ambcol;
    }

    hitValue = result;
}


