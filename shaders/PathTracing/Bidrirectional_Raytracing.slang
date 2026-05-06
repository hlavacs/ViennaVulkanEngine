import Warp;
import raytracing;
import Lightsource;
import Random;
import Frame;
import Defines;
import Surface;
import Materials;
import BSDF;


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
        lightVertex.radiance = 0.0;
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
    //likely needs all pdfs
    lightVertex.radiance = light.emission / (pdf);
    lightVertex.samplePosition = float4(lightSurfacePosition, 1.0);
    lightVertex.selectionPdf = 1.0;
    lightVertex.normal = float4(its.normal, 1.0);
    lightVertex.tangent = float4(its.tangent, 1.0);
    lightVertex.materialIndex = its.materialIdx;
    lightVertex.uv = its.uv;

    return lightVertex;
}

IntersectionRecord getIntersectionRecordFromLightVertex(in LightVertex lightVertex, in IntersectionRecord its) {
    IntersectionRecord itsConnection;

    itsConnection.direction = normalize(its.position - lightVertex.hitPosition.xyz);
    itsConnection.materialIdx = lightVertex.materialIndex;
    itsConnection.normal = lightVertex.normal.xyz;
    itsConnection.position = lightVertex.hitPosition.xyz;
    itsConnection.tangent = lightVertex.tangent.xyz;
    itsConnection.frame = Frame(itsConnection.normal, itsConnection.tangent);
    return itsConnection;
}

EmitterRecord getEmitterRecordFromLightVertex(in LightVertex lightVertex) {
    EmitterRecord emitterRecord;
    emitterRecord.direction = lightVertex.samplePosition.xyz - lightVertex.hitPosition.xyz;
    emitterRecord.distance = length(emitterRecord.direction);
    emitterRecord.intersection = lightVertex.hitPosition.xyz;
    emitterRecord.sampleEmission = lightVertex.radiance.xyz;

    return emitterRecord;
}

EmitterRecord getEmitterRecordFromConnection(in EmitterRecord fromLightSource, in IntersectionRecord its, in IntersectionRecord itsConnection, in MaterialRecord materialRecordConnection) {
    EmitterRecord emitterRecord;
    emitterRecord.direction = -itsConnection.direction;
    emitterRecord.distance = length(its.position - itsConnection.position);
    emitterRecord.intersection = itsConnection.position;
    float3 bsdfEvalConnection = getUnoccludedLightContribution(itsConnection, materialRecordConnection, fromLightSource);
    //likely needs to be devided by distance squared
    emitterRecord.sampleEmission = bsdfEvalConnection;

    return emitterRecord;
}

float3 getUnoccludedLightContributionFromConnection(in IntersectionRecord its, in MaterialRecord material, in LightVertex lightVertex) {

    IntersectionRecord itsConnection = getIntersectionRecordFromLightVertex(lightVertex, its);
    Material materialConnection = materials[itsConnection.materialIdx];
    MaterialRecord materialRecordConnection = getMaterialProperties(materialConnection, textures, itsConnection.uv, itsConnection.frame);

    EmitterRecord emitterRecord = getEmitterRecordFromLightVertex(lightVertex);
    EmitterRecord emitterRecordFromConnection = getEmitterRecordFromConnection(emitterRecord, its, itsConnection, materialRecordConnection);

    float3 radiance = getUnoccludedLightContribution(its, material, emitterRecordFromConnection);

    return radiance;
}

float3 getOccludedLightContributionFromConnection(in IntersectionRecord its, in MaterialRecord material, in LightVertex lightVertex) {
    IntersectionRecord itsConnection = getIntersectionRecordFromLightVertex(lightVertex, its);
    Material materialConnection = materials[itsConnection.materialIdx];
    MaterialRecord materialRecordConnection = getMaterialProperties(materialConnection, textures, itsConnection.uv, itsConnection.frame);

    EmitterRecord emitterRecord = getEmitterRecordFromLightVertex(lightVertex);
    EmitterRecord emitterRecordFromConnection = getEmitterRecordFromConnection(emitterRecord, its, itsConnection, materialRecordConnection);

    if (isLightOccluded(its, emitterRecordFromConnection)) {
        return float3(0.0);
    }

    float3 radiance = getUnoccludedLightContribution(its, material, emitterRecordFromConnection);

    return radiance;
}