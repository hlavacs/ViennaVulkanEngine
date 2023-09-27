#version 460
#extension GL_NV_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_scalar_block_layout : enable

#include "../common_defines.glsl"
#include "../light.glsl"

layout(location = 0) rayPayloadInNV vec3 hitValue;
layout(location = 2) rayPayloadNV bool isShadowed;
hitAttributeNV vec3 attribs;

layout(set = 0, binding = 0) uniform cameraUBO_t { cameraData_t data; } cameraUBO;

layout(set = 1, binding = 0) uniform lightUBO_t { lightData_t data[3]; } lightUBOs;

layout(set = 3, binding = 0) uniform accelerationStructureNV topLevelAS;

layout(set = 4, binding = 0) buffer Vertices { vec4 v[]; } vertices[];
layout(set = 4, binding = 1) buffer Indices { uint i[]; } indices[];

layout(set = 5, binding = 0) buffer objectUBO_t { vec4 data[]; } objectUBOs[];

layout(set = 6, binding = 0) uniform sampler2D texSamplerArray[RESOURCEARRAYLENGTH];
layout(set = 6, binding = 1) uniform sampler2D normalSamplerArray[RESOURCEARRAYLENGTH];


// Number of vec4 values used to represent a vertex
uint vertexSize = 3;
Vertex unpackVertex(uint objId, uint index)
{
    Vertex v;

    vec4 d0 = vertices[objId].v[vertexSize * index + 0];
    vec4 d1 = vertices[objId].v[vertexSize * index + 1];
    vec4 d2 = vertices[objId].v[vertexSize * index + 2];

    v.pos = d0.xyz;
    v.normal = vec3(d0.w, d1.x, d1.y);
    v.tangent = vec3(d1.z, d1.w, d2.x);
    v.texCoord = vec2(d2.y, d2.z);
    v.entityId = floatBitsToInt(d2.w);
    return v;
};

objectData_t unpackObjectData(uint objId)
{
    objectData_t objData;

    vec4 d0 = objectUBOs[objId].data[0];
    vec4 d1 = objectUBOs[objId].data[1];
    vec4 d2 = objectUBOs[objId].data[2];
    vec4 d3 = objectUBOs[objId].data[3];
    vec4 d4 = objectUBOs[objId].data[4];
    vec4 d5 = objectUBOs[objId].data[5];
    vec4 d6 = objectUBOs[objId].data[6];
    vec4 d7 = objectUBOs[objId].data[7];
    vec4 d8 = objectUBOs[objId].data[8];
    vec4 d9 = objectUBOs[objId].data[9];
    vec4 d10 = objectUBOs[objId].data[10];

    objData.model = mat4(d0, d1, d2, d3);
    objData.modelInvTrans = mat4(d4, d5, d6, d7);
    objData.color = d8;
    objData.param = d9;
    objData.iparam = ivec4(floatBitsToInt(d10.x), floatBitsToInt(d10.y), floatBitsToInt(d10.z), floatBitsToInt(d10.w));
    return objData;
};
/*
lightData_t unpackLight(uint objId)
{
	lightData_t lightData; 

	vec4 d0 = lightUBOs[objId].data[0];
    vec4 d1 = lightUBOs[objId].data[1];
	vec4 d2 = lightUBOs[objId].data[2];
	vec4 d3 = lightUBOs[objId].data[3];
	vec4 d4 = lightUBOs[objId].data[4];
	vec4 d5 = lightUBOs[objId].data[5];
	vec4 d6 = lightUBOs[objId].data[6];
	vec4 d7 = lightUBOs[objId].data[7];
	vec4 d8 = lightUBOs[objId].data[8];

	lightData.itype = ivec4(floatBitsToInt(d0.x),floatBitsToInt(d0.y),floatBitsToInt(d0.z),floatBitsToInt(d0.w));
	lightData.lightModel = mat4(d1, d2, d3, d4);
	lightData.col_ambient = d5;
	lightData.col_diffuse = d6;
	lightData.col_specular = d7;
	lightData.param = d8;

	return lightData;
};
*/
void main()
{
    ivec3 ind = ivec3(indices[gl_InstanceID].i[3 * gl_PrimitiveID], indices[gl_InstanceID].i[3 * gl_PrimitiveID + 1], indices[gl_InstanceID].i[3 * gl_PrimitiveID + 2]);

    uint objId = gl_InstanceID;

    Vertex v0 = unpackVertex(objId, ind.x);
    Vertex v1 = unpackVertex(objId, ind.y);
    Vertex v2 = unpackVertex(objId, ind.z);

    objectData_t objectUBO = unpackObjectData(objId);

    const vec3 barycentrics = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);

    vec3 fragNormalL = normalize(v0.normal * barycentrics.x + v1.normal * barycentrics.y + v2.normal * barycentrics.z);
    vec3 fragPosL = normalize(v0.pos * barycentrics.x + v1.pos * barycentrics.y + v2.pos * barycentrics.z);
    vec3 fragTangentL = normalize(v0.tangent * barycentrics.x + v1.tangent * barycentrics.y + v2.tangent * barycentrics.z);

    vec3 fragPosW        = (objectUBO.model         * vec4(fragPosL, 1.0)).xyz;
    vec3 fragNormalW    = (objectUBO.modelInvTrans * vec4(fragNormalL, 1.0)).xyz;
    vec3 fragTangentW   = (objectUBO.modelInvTrans * vec4(fragTangentL, 0.0)).xyz;
    vec2 fragTexCoord    = (v0.texCoord * barycentrics.x + v1.texCoord * barycentrics.y + v2.texCoord * barycentrics.z);


    vec3 camPosW    = cameraUBO.data.camModel[3].xyz;
    vec4 texParam   = objectUBO.param;
    vec2 texCoord   = (fragTexCoord + texParam.zw)*texParam.xy;
    ivec4 iparam    = objectUBO.iparam;
    uint resIdx     = iparam.x % RESOURCEARRAYLENGTH;
    vec3 fragColor = texture(texSamplerArray[resIdx], texCoord).xyz;

    //TBN matrix
    vec3 N        = normalize(fragNormalW);
    vec3 T        = normalize(fragTangentW);
    T             = normalize(T - dot(T, N)*N);
    vec3 B        = normalize(cross(T, N));
    mat3 TBN      = mat3(T, B, N);
    vec3 mapnorm  = normalize(texture(normalSamplerArray[resIdx], texCoord).xyz*2.0 - 1.0);
    vec3 normalW  = normalize(TBN * mapnorm);


    vec3 result = vec3(0, 0, 0);
    for (int i = 0; i < 3; i++)
    {
        lightData_t lightUBO = lightUBOs.data[i];

        int  lightType  = lightUBO.itype[0];
        vec3 lightPosW  = lightUBO.lightModel[3].xyz;
        vec3 lightDirW  = normalize(lightUBO.lightModel[2].xyz);
        float nfac      = dot(fragNormalW, -lightDirW)<0? 0.5:1;
        vec4 lightParam = lightUBO.param;


        // colors
        vec3 ambcol  = lightUBO.col_ambient.xyz;
        vec3 diffcol = lightUBO.col_diffuse.xyz;
        vec3 speccol = lightUBO.col_specular.xyz;

        result += fragColor * ambcol;

        /*
        if( lightType == LIGHT_DIR ) {
            result +=   dirlight( lightType, camPosW,
                                    lightDirW, lightParam, 1.0,
                                    ambcol, diffcol, speccol,
                                    fragPosW, normalW, fragColor);
        }


        if( lightType == LIGHT_POINT ) {
            result +=   pointlight( lightType, camPosW,
                                    lightPosW, lightParam, 1.0,
                                    ambcol, diffcol, speccol,
                                    fragPosW, normalW, fragColor);
        }

        if( lightType == LIGHT_SPOT ) {
            result +=  spotlight( lightType, camPosW,
                                    lightPosW, lightDirW, lightParam, 1.0,
                                    ambcol, diffcol, speccol,
                                    fragPosW, normalW, fragColor);
        }

        if( lightType == LIGHT_AMBIENT ) {
            result += fragColor * ambcol;
        }*/
    }

    hitValue = result;
}


