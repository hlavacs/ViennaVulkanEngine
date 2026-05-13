import Warp;
import raytracing;
import Lightsource;
import Random;
import Frame;
import Defines;
import Surface;
import Materials;
import BSDF;

[[vk::binding(5, 1)]]
RWStructuredBuffer<LightVertex> lightVertexCache;

[[vk::binding(6, 1)]]
cbuffer UniformBufferBidirectional
{
    uint lightVertexCacheSize;
};

struct LightVertex {
    float4 radiance;
    float4 samplePosition;
    float4 hitPosition;
    float4 normal;
    float4 tangent;
    float2 uv;
    uint lightIndex;
    uint materialIndex;
    uint age;
    uint lastReevaluation;
    float selectionPdf;
    float importance;
}


    // this only works with consistent instance indices which is bad
/*
struct LightVertex {
    float4 radiance;
    float4 samplePosition;
    float4 hitPosition; // only required for light path validation

    float4 barycentrics;
    uint triangleIdx;
    uint instanceIdx;

    uint lightIndex;
    uint age;
    float selectionPdf;
    float importance;
}
*/

LightVertex generateLightVertex(inout Sampler sampler) {
    LightVertex lightVertex;
    
    float pdf = 0.0;
    uint lightIndex = 0;
    LightSource light = chooseRandomLightSourceWeighted(lightBuffer, sampler, pdf, lightIndex);

    //we assume sphere light for now!!!!!!!!
    float2 sample = sampler.UniformFloat2();
    float3 lightSurfaceDirection = squareToUniformSphere(sample);
    float lightSurfaceSelectionPdf = squareToUniformSpherePdf(lightSurfaceDirection);
    float3 lightSurfacePosition = light.position.xyz + (lightSurfaceDirection * light.radius);

    Frame lightFrame = Frame(lightSurfaceDirection);

    float3 lightDirectionLocal = squareToUniformHemisphere(sample);
    float lightDirectionSelectionPdf = squareToUniformHemispherePdf(lightSurfaceDirection);
    float3 lightSampleDirection = lightFrame.toWorld(lightDirectionLocal);

    RayDesc ray;
    ray.Origin = lightSurfacePosition;
    ray.Direction = normalize(lightSampleDirection);
    ray.TMin = 0.0;
    ray.TMax = INFINITE;

    HitPayload payload;
    const uint rayFlags = 0;
    TraceRay(topLevelAS, rayFlags, 0xff, 0, 0, 0, ray, payload);

    if (!payload.hit) {
        lightVertex.age = 0;
        lightVertex.hitPosition = float4(0.0);
        lightVertex.importance = 0.0;
        lightVertex.lightIndex = lightIndex;
        lightVertex.radiance = float4(0.0);
        lightVertex.samplePosition = float4(lightSurfacePosition, 1.0);
        lightVertex.selectionPdf = 1.0;
        lightVertex.normal = float4(0.0);
        lightVertex.tangent = float4(0.0);
        lightVertex.materialIndex = 0;
        lightVertex.uv = float2(0.0);

        return lightVertex;
    }

    IntersectionRecord its = getIntersectionInfo(payload, ray);

    lightVertex.age = 0;
    lightVertex.hitPosition = float4(its.position, 1.0);
    lightVertex.importance = 0.0;
    lightVertex.lightIndex = lightIndex;
    // likely needs all pdfs
    //lightVertex.radiance = light.emission / (pdf * lightSurfaceSelectionPdf * lightDirectionSelectionPdf);
    lightVertex.radiance = light.emission / (pdf);
    //lightVertex.radiance = light.emission / (pdf);
    lightVertex.samplePosition = float4(lightSurfacePosition, 1.0);
    lightVertex.selectionPdf = 1.0;
    lightVertex.normal = float4(its.normal, 1.0);
    lightVertex.tangent = float4(its.tangent, 1.0);
    lightVertex.materialIndex = its.materialIdx;
    lightVertex.uv = its.uv;

    return lightVertex;
}

bool isLightVertexValid(uint lightVertexIndex) {
    LightVertex lightVertex = lightVertexCache[lightVertexIndex];
    return length(lightVertex.radiance) >= 0.0;
}

IntersectionRecord getIntersectionRecordFromLightVertex(in LightVertex lightVertex, in IntersectionRecord its) {
    IntersectionRecord itsConnection;

    // its.direction is the reay direction aka ingoing instead of outgoing bsdf inverts this
    // itsConnection.direction = normalize(its.position - lightVertex.hitPosition.xyz);
    itsConnection.direction = normalize(lightVertex.hitPosition.xyz - its.position);
    itsConnection.materialIdx = lightVertex.materialIndex;
    itsConnection.normal = lightVertex.normal.xyz;
    itsConnection.position = lightVertex.hitPosition.xyz;
    itsConnection.tangent = lightVertex.tangent.xyz;
    itsConnection.frame = Frame(itsConnection.normal, itsConnection.tangent);
    return itsConnection;
}

EmitterRecord getEmitterRecordFromLightVertex(in LightVertex lightVertex) {
    EmitterRecord emitterRecord;
    emitterRecord.direction = normalize(lightVertex.samplePosition.xyz - lightVertex.hitPosition.xyz);
    emitterRecord.distance = length(lightVertex.samplePosition.xyz - lightVertex.hitPosition.xyz);
    emitterRecord.intersection = lightVertex.hitPosition.xyz;
    emitterRecord.sampleEmission = lightVertex.radiance.xyz;

    return emitterRecord;
}

/*
EmitterRecord getEmitterRecordFromConnection(in EmitterRecord fromLightSource, in IntersectionRecord its, in IntersectionRecord itsConnection, in MaterialRecord materialRecordConnection) {
    EmitterRecord emitterRecord;
    emitterRecord.direction = -itsConnection.direction;
    emitterRecord.distance = length(its.position - itsConnection.position);
    emitterRecord.intersection = itsConnection.position;
    float3 bsdfEvalConnection = getUnoccludedLightContribution(itsConnection, materialRecordConnection, fromLightSource);
    // likely needs to be devided by distance squared

    float cosThetaLight = saturate(dot(itsConnection.normal, emitterRecord.direction));

    emitterRecord.sampleEmission = bsdfEvalConnection * (cosThetaLight / (emitterRecord.distance * emitterRecord.distance));

    return emitterRecord;
}
    */


EmitterRecord getEmitterRecordFromConnection(in EmitterRecord fromLightSource, in IntersectionRecord its, in IntersectionRecord itsConnection, in MaterialRecord materialRecordConnection) {
    EmitterRecord emitterRecord;
    // emitterRecord.direction = itsConnection.direction;
    emitterRecord.direction = normalize(itsConnection.position - its.position);
    emitterRecord.distance = length(itsConnection.position - its.position);
    emitterRecord.intersection = itsConnection.position;
    float3 bsdfEvalConnection = getUnoccludedLightContribution(itsConnection, materialRecordConnection, fromLightSource);
    // likely needs to be devided by distance squared

    //not sure if this is correct, but it makes the result less biased looking
    float cosThetaLight = saturate(dot(itsConnection.normal, -emitterRecord.direction));
    //float cosThetaLight = 1.0;

    //emitterRecord.sampleEmission = bsdfEvalConnection * (cosThetaLight / (emitterRecord.distance * emitterRecord.distance));

    float distSq = max(emitterRecord.distance * emitterRecord.distance, 0.01);

    emitterRecord.sampleEmission = bsdfEvalConnection * (cosThetaLight / (distSq));

    return emitterRecord;
}

float3 getUnoccludedLightContributionFromConnection(in IntersectionRecord its, in MaterialRecord material, in uint lightVertexIndex) {

    LightVertex lightVertex = lightVertexCache[lightVertexIndex];

    IntersectionRecord itsConnection = getIntersectionRecordFromLightVertex(lightVertex, its);

    //low distance is clearly a problem but better ffix needed!
    if (length(itsConnection.position - its.position) < 0.01) {
        return float3(0.0);
    }


    Material materialConnection = materials[itsConnection.materialIdx];
    MaterialRecord materialRecordConnection = getMaterialProperties(materialConnection, textures, itsConnection.uv, itsConnection.frame);

    EmitterRecord emitterRecord = getEmitterRecordFromLightVertex(lightVertex);
    EmitterRecord emitterRecordFromConnection = getEmitterRecordFromConnection(emitterRecord, its, itsConnection, materialRecordConnection);

    if (dot(itsConnection.normal, -emitterRecordFromConnection.direction) < 0.0) {
        return float3(0.0);
    }

    if (dot(emitterRecordFromConnection.direction, its.normal) < 1e-4f) {
        return float3(0.0);
    }

    // check if we hit a backface
    /*
    if (dot(materialRecordConnection.shadingFrame.n, -emitterRecordFromConnection.direction) < 0.0) {
        return float3(0.0);
    }
        */

    //normals are flipped on backfacing geometry resulting in light leaking through
    float3 radiance = getUnoccludedLightContribution(its, material, emitterRecordFromConnection);

    return radiance;
}

// this code sometimes still returns false posiitives iit is completly unclear why
bool isSurfacePointOccludedTest(in IntersectionRecord its, in IntersectionRecord itsConnection, EmitterRecord emitterRecord) {
    // 1. Use a robust offset to prevent self-intersection
    RayDesc shadowRay;
    // shadowRay.Origin = its.position + (its.normal * 0.001);
    shadowRay.Origin = its.position;
    shadowRay.Direction = normalize(emitterRecord.direction);
    shadowRay.TMin = 0.001;
    // 2. Stop the ray just BEFORE it reaches the target point
    shadowRay.TMax = emitterRecord.distance - 0.001;

    // 3. Horizon check (using a small epsilon to allow grazing lights)
    if (dot(shadowRay.Direction, its.normal) < 1e-4f) {
        return true;
    }



    HitPayload payload;
    payload.hit = false; // Ensure this is initialized!

    // 4. Optimization: Use flags to stop at the very first hit
    const uint rayFlags = RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH;

    TraceRay(topLevelAS, rayFlags, 0xff, 0, 0, 0, shadowRay, payload);

    // If the ray hit anything before reaching the target, it's occluded
    return payload.hit;
}

float3 getOccludedLightContributionFromConnection(in IntersectionRecord its, in MaterialRecord material, in uint lightVertexIndex) {

    LightVertex lightVertex = lightVertexCache[lightVertexIndex];

    IntersectionRecord itsConnection = getIntersectionRecordFromLightVertex(lightVertex, its);

    if (length(itsConnection.position - its.position) < 0.01) {
        return float3(0.0);
    }



    Material materialConnection = materials[itsConnection.materialIdx];
    MaterialRecord materialRecordConnection = getMaterialProperties(materialConnection, textures, itsConnection.uv, itsConnection.frame);

    EmitterRecord emitterRecord = getEmitterRecordFromLightVertex(lightVertex);
    EmitterRecord emitterRecordFromConnection = getEmitterRecordFromConnection(emitterRecord, its, itsConnection, materialRecordConnection);

    //check if we hit a backface
    if (dot(itsConnection.normal, -emitterRecordFromConnection.direction) < 0.0) {
        return float3(0.0);
    }

    /*
    if (dot(materialRecordConnection.shadingFrame.n, -emitterRecordFromConnection.direction) < 0.0) {
        return float3(0.0);
    }
        */

    if (isSurfacePointOccludedTest(its, itsConnection, emitterRecordFromConnection)) {
        return float3(0.0);
    }

    float3 radiance = getUnoccludedLightContribution(its, material, emitterRecordFromConnection);

    return radiance;
}