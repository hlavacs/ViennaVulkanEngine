import raytracing;
import Surface;
import Materials;
import Frame;
import Random;
import Warp;
import IR_Defines;
import IR_Bindings;
import Lightsource;
import Defines;
import IR_Common;

[[vk::binding(0, 2)]]
RWTexture2D<float4> inNormal;

[[vk::binding(1, 2)]]
RWTexture2D<float4> inPosition;

void getInitialSurfaceData(in int2 launchID, in int2 launchSize, inout IntersectionRecord its) {
    RayDesc cameraRay = getInitialRay(launchID, launchSize);

    its.position = inPosition[int2(launchID)].xyz;
    its.normal = inNormal[int2(launchID)].xyz;  
    its.direction = cameraRay.Direction;

    if (dot(its.normal, its.direction) > 0.0) {
        its.normal *= -1.0;
    }

    its.frame = Frame(its.normal);
}

RayDesc createRay(in IntersectionRecord its, float3 castDirection) {
    // Inputs: intersection and cast direction. Output: ray offset based on side.
    RayDesc ray;
    // ray.Origin = its.position + (its.normal * 0.001);
    ray.Origin = offset_ray(its.position, its.normal);

    ray.Direction = normalize(castDirection);
    ray.TMin = 0.001;      // Minimum distance to avoid self-intersection
    ray.TMax = INFINITE; // Maximum distance (infinite for primary rays)

    return ray;
}

RayDesc generateVPLRay(in IntersectionRecord its, inout Sampler sampler) {
    //always assume diffuse shader for VPL generation
    float2 sample = sampler.UniformFloat2();
    float3 woShading = squareToCosineHemisphere(sample);
    float3 woWorld = its.frame.toWorld(woShading);

    return createRay(its, woWorld);
}

RayDesc mutateVPLRay(in IntersectionRecord its, in VPLReconnectionInfo vpl, inout Sampler sampler, float mutation) {
    // always assume diffuse shader for VPL generation
    float3 woWorld = normalize(vpl.position.xyz - its.position);
    float3 woShading = its.frame.toLocal(woWorld);

    float2 generationSample = cosineHemisphereToSquare(woShading);
    float2 sample = sampler.UniformFloat2();

    float2 mutatedSample = generationSample + mutation * (sample - float2(0.5));

    mutatedSample = float2(saturate(mutatedSample.x), mutatedSample.y);

    float3 woMutatedShading = squareToCosineHemisphere(mutatedSample);
    float3 woMutatedWorld = its.frame.toWorld(woMutatedShading);

    return createRay(its, woMutatedWorld);
}

void createVPL(in IntersectionRecord its, in RayDesc ray, out VPLReconnectionInfo vpl, out VPLShading shadingInfo) {
    HitPayload payload;
    const uint rayFlags = 0;
    TraceRay(topLevelAS, rayFlags, 0xff, 0, 0, 0, ray, payload);

    if (!payload.hit) {
        shadingInfo.radiance = float3(0.0);
        vpl.weightedRadiance = float3(0.0);
        return; 
    }

    IntersectionRecord itsSecundary = getIntersectionInfo(payload, ray);
    Material material = materials[itsSecundary.materialIdx];
    MaterialRecord materialRecordSecundary = getMaterialProperties(material, textures, itsSecundary.uv, itsSecundary.frame);

    vpl.position = itsSecundary.position;
    vpl.normal = itsSecundary.normal;
    shadingInfo.albedo = materialRecordSecundary.albedo;
}

float3 getUnoccludedLightContributionDiffuse(in VPLReconnectionInfo vpl, in VPLShading shadingInfo, in EmitterRecord emitterRecord) {
    // diffuse shader only, without expansive call to full bsdf pipline!
    float cosine = saturate(dot(emitterRecord.direction, vpl.normal.xyz));

    //float3 diffuseEval = shadingInfo.albedo / M_PI * cosine;
    //for testing purposes albedo is set to 0.5 
    float3 diffuseEval = float3(0.5) / M_PI * cosine;

    float3 radiance = emitterRecord.sampleEmission * diffuseEval;

    if (any(isnan(radiance)) || any(isinf(radiance))) {
        radiance = float3(0.0);
    }

    return max(0.0, radiance);
}

float3 lightRadiance(in uint lightIndex, in VPLReconnectionInfo vpl, in VPLShading shadingInfo, inout Sampler sampler) {
    LightSource light = lightBuffer[lightIndex];
    EmitterRecord record = sampleLightSource(light, vpl.position.xyz, sampler);

    // stupid hack
    float3 lightPosition = vpl.position.xyz + (record.direction * record.distance);

    bool occluded = isPointOccluded(vpl.position.xyz, lightPosition, vpl.normal.xyz);

    if (occluded) {
        return 0.0;
    }

    return getUnoccludedLightContributionDiffuse(vpl, shadingInfo, record);
}

void shadeVPL(in VPLReconnectionInfo vpl, inout VPLShading shadingInfo, inout Sampler sampler, uint sampleCount) {
    float3 radiance = float3(0.0);
    float pdf = 1.0 / lightCount;

    for (uint i = 0; i < sampleCount; i++) {
        uint lightIndex = sampler.UniformUIntFast(0, lightCount - 1);
        radiance += lightRadiance(lightIndex, vpl, shadingInfo, sampler) / pdf;
    }

    shadingInfo.radiance = radiance / (float)sampleCount;
}

void setWeightedRadiance(inout VPLReconnectionInfo vpl, in VPLShading shadingInfo, in IntersectionRecord itsCamera) {
    float3 outVec = vpl.position.xyz - itsCamera.position;
    float3 outDirection = normalize(outVec);
    float distance = length(outVec);

    float cosinePrimary = saturate(dot(itsCamera.normal, outDirection));

    if (isnan(cosinePrimary) || isinf(cosinePrimary) || cosinePrimary < 0.0 || cosinePrimary > 1.0) {
        vpl.weightedRadiance = float3(0.0,0.0,0.0);
        return;
    }

    float solidAnglePDF = cosinePrimary / (M_PI);

    float cosineSecondary = dot(vpl.normal, -outDirection);

    //I divide by the pdf so it will be like multiplying by r^2/cos
    float areaPDF = solidAnglePDF * (cosineSecondary / (distance * distance));

    //vpl.weightedRadiance = shadingInfo.radiance / areaPDF;

    vpl.weightedRadiance = shadingInfo.radiance * float3(100.0);

    //vpl.weightedRadiance = float3(100.0);
}