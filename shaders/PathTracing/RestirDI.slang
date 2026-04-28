import Random;
import raytracing;
import Lightsource;
import Surface;
import Materials;
import BSDF;
import Reservoir;

bool UpdateReservoir(inout Reservoir reservoir, uint light, float4 samplePosition, float w, uint count, inout Sampler sampler)
{
    // 1. Sanity check for NaN/Inf
    if (isnan(w) || isinf(w)) w = 0.0;


    reservoir.W_sum += w;
    reservoir.M += count;

    if (sampler.UniformFloat() < (w / reservoir.W_sum))
    {
        reservoir.lightSelected = light;
        reservoir.samplepoint = samplePosition;
        return true;
    }

    return false;
}

bool IsReservoirValid(Reservoir reservoir) {
    return reservoir.M > 0 && reservoir.W_sum > 0;
}

float unoccludedLightRadianceCenter(in uint lightIndex, in IntersectionRecord its, in MaterialRecord material, inout Sampler sampler) {
    LightSource light = lightBuffer[lightIndex];
    EmitterRecord record = sampleLightSourceCenter(light, its.position, sampler);
    return length(getUnoccludedLightContribution(its, material, record));
}

float unoccludedLightRadiance(in uint lightIndex, in IntersectionRecord its, in MaterialRecord material, inout Sampler sampler) {
    LightSource light = lightBuffer[lightIndex];
    EmitterRecord record = sampleLightSource(light, its.position, sampler);
    return length(getUnoccludedLightContribution(its, material, record));
}

float3 groundTruthLightRadiance(in uint lightIndex, in IntersectionRecord its, in MaterialRecord material, inout Sampler sampler) {
    LightSource light = lightBuffer[lightIndex];
    EmitterRecord record = sampleLightSource(light, its.position, sampler);

    bool occluded = isLightOccluded(its, record);

    if (occluded) {
        return 0.0;
    }

    return getUnoccludedLightContribution(its, material, record);
}

float occludedLightRadianceCenter(in uint lightIndex, in IntersectionRecord its, in MaterialRecord material, inout Sampler sampler) {
    LightSource light = lightBuffer[lightIndex];
    EmitterRecord record = sampleLightSourceCenter(light, its.position, sampler);

    bool occluded = isLightOccluded(its, record);

    if (occluded) {
        return 0.0;
    }

    return length(getUnoccludedLightContribution(its, material, record));
}

bool isLightOccludedCenter(in uint lightIndex, in IntersectionRecord its, inout Sampler sampler) {
    LightSource light = lightBuffer[lightIndex];
    EmitterRecord emitterRecord = sampleLightSourceCenter(light, its.position, sampler);
    bool occluded = isLightOccluded(its, emitterRecord);

    return  occluded;
}

float sampleUnoccludedLightRadiance(in uint lightIndex, in IntersectionRecord its, in MaterialRecord material, inout Sampler sampler, out float4 samplePosition) {
    LightSource light = lightBuffer[lightIndex];
    EmitterRecord record = sampleLightSource(light, its.position, sampler);
    samplePosition = float4(record.intersection, 0.0);
    return length(getUnoccludedLightContribution(its, material, record));
}

float revaluateUnoccludedLightRadiance(in Reservoir reservoir, in IntersectionRecord its, in MaterialRecord material) {
    LightSource light = lightBuffer[reservoir.lightSelected];
    EmitterRecord record = resampleLightSource(light, its.position, reservoir.samplepoint.xyz);
    return length(getUnoccludedLightContribution(its, material, record));
}

float3 revaluateOccludedLightRadiance(in Reservoir reservoir, in IntersectionRecord its, in MaterialRecord material) {
    LightSource light = lightBuffer[reservoir.lightSelected];
    EmitterRecord record = resampleLightSource(light, its.position, reservoir.samplepoint.xyz);

    bool occluded = isLightOccluded(its, record);

    if (occluded) {
        return 0.0;
    }

    return getUnoccludedLightContribution(its, material, record);
}

float3 resampleOccludedLightRadiance(in Reservoir reservoir, in IntersectionRecord its, in MaterialRecord material, inout Sampler sampler) {
    LightSource light = lightBuffer[reservoir.lightSelected];
    EmitterRecord record = sampleLightSource(light, its.position, sampler);

    bool occluded = isLightOccluded(its, record);

    if (occluded) {
        return 0.0;
    }

    return getUnoccludedLightContribution(its, material, record);
}

Reservoir CombineReservoirs(in Reservoir reservoir1, in Reservoir reservoir2, in IntersectionRecord its, in MaterialRecord material, inout Sampler sampler)
{
    Reservoir reservoir = createEmptyReservoir();

    float pHat1 = revaluateUnoccludedLightRadiance(reservoir1, its, material);
    // this simplifies to w_sum if the intersection point is identical (lightWeight = 1/pHat * wsum / M) (not the case with temporal or spatial reuse)
    float w1 = pHat1 * reservoir1.lightWeight * float(reservoir1.M);
    if(IsReservoirValid(reservoir1)){
        UpdateReservoir(reservoir, reservoir1.lightSelected, reservoir1.samplepoint, w1, reservoir1.M, sampler);
    }

    float pHat2 = revaluateUnoccludedLightRadiance(reservoir2, its, material);
    float w2 = pHat2 * reservoir2.lightWeight * float(reservoir2.M);
    if (IsReservoirValid(reservoir2)) {
        UpdateReservoir(reservoir, reservoir2.lightSelected, reservoir2.samplepoint, w2, reservoir2.M, sampler);
    }

    float pHat = revaluateUnoccludedLightRadiance(reservoir, its, material);
    if (pHat <= 0.0 || reservoir.M == 0) {
        reservoir.lightWeight = 0.0;
    } else {
        reservoir.lightWeight = (1.0 / max(pHat, 0.001)) * (reservoir.W_sum / float(reservoir.M));
    }

    return reservoir;
}