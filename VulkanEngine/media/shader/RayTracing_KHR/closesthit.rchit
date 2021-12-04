#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_scalar_block_layout : enable

#include "../common_defines.glsl"
#include "../light.glsl"

// override number of resources
#undef RESOURCEARRAYLENGTH
#define RESOURCEARRAYLENGTH 256

layout(location = 0) rayPayloadInEXT hitPayload prd;
layout(location = 1) rayPayloadEXT bool isShadowed;
hitAttributeEXT vec3 attribs;

layout(push_constant) uniform PushConsts {
    RTPushConstants_t data;
} pushConsts;

layout(set = 0, binding = 0) uniform cameraUBO_t {
    cameraData_t data;
} cameraUBO;


layout(set = 1, binding = 0) uniform lightUBO_t {
    lightData_t data;
} lightUBO;

layout(set = 3, binding = 0) uniform accelerationStructureEXT topLevelAS;

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
}


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
    vec4 d11 = objectUBOs[objId].data[11];
    vec4 d12 = objectUBOs[objId].data[12];
    vec4 d13 = objectUBOs[objId].data[13];
    vec4 d14 = objectUBOs[objId].data[14];

    objData.model = mat4(d0, d1, d2, d3);
    objData.modelTrans = mat4(d4, d5, d6, d7);
    objData.modelInvTrans = mat4(d8, d9, d10, d11);
    objData.color = d12;
    objData.param = d13;
    objData.iparam = ivec4(floatBitsToInt(d14.x), floatBitsToInt(d14.y), floatBitsToInt(d14.z), floatBitsToInt(d14.w));
    return objData;
}

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

    int  lightType  = lightUBO.data.itype[0];
    vec3 camPosW    = prd.rayDir;
    vec3 lightPosW  = lightUBO.data.lightModel[3].xyz;
    vec3 lightDirW  = normalize(lightUBO.data.lightModel[2].xyz);
    float nfac      = dot(fragNormalW, -lightDirW)<0? 0.5:1;
    vec4 lightParam = lightUBO.data.param;
    vec4 texParam   = objectUBO.param;
    vec2 texCoord   = (fragTexCoord + texParam.zw)*texParam.xy;
    ivec4 iparam    = objectUBO.iparam;
    uint resIdx     = iparam.x % RESOURCEARRAYLENGTH;

    //TBN matrix
    vec3 N        = normalize(fragNormalW);
    vec3 T        = normalize(fragTangentW);
    T             = normalize(T - dot(T, N)*N);
    vec3 B        = normalize(cross(T, N));
    mat3 TBN      = mat3(T, B, N);
    vec3 mapnorm  = normalize(texture(normalSamplerArray[resIdx], texCoord).xyz*2.0 - 1.0);
    vec3 normalW  = iparam.y == 1?normalize(TBN * mapnorm):fragNormalW;
    vec3 fragColor = texture(texSamplerArray[resIdx], texCoord).xyz;

    isShadowed = true;


    if (pushConsts.data.shadowEnabled && lightType != LIGHT_AMBIENT) {
        float tmin = 0.001;
        float tmax = 100.0;
        vec3 origin = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
        vec3  rayDir = vec3(0.0);

        if (lightType == LIGHT_DIR) {
            rayDir = - normalize(lightDirW);
        }
        if (lightType == LIGHT_POINT || lightType == LIGHT_SPOT) {
            rayDir = lightPosW - origin;
            tmax = rayDir.length();
        }

        uint  flags =  gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT;
        traceRayEXT(topLevelAS, // acceleration structure
        flags, // rayFlags
        0xFF, // cullMask
        0, // sbtRecordOffset
        0, // sbtRecordStride
        1, // missIndex
        origin, // ray origin
        tmin, // ray min range
        rayDir, // ray direction
        tmax, // ray max range
        1// payload (location = 1)
        );
    }
    else
    {
        isShadowed = false;
    }


    // colors
    vec3 ambcol  = lightUBO.data.col_ambient.xyz;
    vec3 diffcol = lightUBO.data.col_diffuse.xyz;
    vec3 speccol = lightUBO.data.col_specular.xyz;

    vec3 result = fragColor * ambcol;
    if (!isShadowed) {


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
            fragPosW, fragNormalW, fragColor);
        }

        if (lightType == LIGHT_SPOT) {
            result +=  spotlight(lightType, camPosW,
            lightPosW, lightDirW, lightParam, 1.0,
            ambcol, diffcol, speccol,
            fragPosW, normalW, fragColor);
        }
    }

    prd.hitValue += result * prd.attenuation;

    if (pushConsts.data.reflectionsEnabled && prd.depth < 1)
    {
        prd.attenuation *= 0.1;
        prd.depth++;
        prd.rayOrigin = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
        prd.rayDir = reflect(prd.rayDir, normalW);
        traceRayEXT(topLevelAS, // acceleration structure
        gl_RayFlagsNoneEXT, // rayFlags
        0xFF, // cullMask
        0, // sbtRecordOffset
        0, // sbtRecordStride
        0, // missIndex
        prd.rayOrigin, // ray origin
        0.1, // ray min range
        prd.rayDir, // ray direction
        1000.0, // ray max range
        0// payload (location = 0)
        );
        prd.depth--;
    }
}


