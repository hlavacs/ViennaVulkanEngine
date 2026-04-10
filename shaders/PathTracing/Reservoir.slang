import Random;
import raytracing;
import Lightsource;
import Surface;
import Materials;

struct Reservoir
{
    uint lightSelected; // index of most important light
    float lightWeight;            // light weight
    float W_sum; // sum of all weights for all lights processed
    uint M; // number of lights processed for this reservoir
};

bool UpdateReservoir(inout Reservoir reservoir, uint light, float w, uint count, inout Sampler sampler)
{
    // 1. Sanity check for NaN/Inf
    if (isnan(w) || isinf(w)) w = 0.0;


    reservoir.W_sum += w;
    reservoir.M += count;

    if (sampler.UniformFloat() < (w / reservoir.W_sum))
    {
        reservoir.lightSelected = light;
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

float occludedLightRadianceMixed(in uint lightIndex, in IntersectionRecord its, in MaterialRecord material, inout Sampler sampler) {
    LightSource light = lightBuffer[lightIndex];
    EmitterRecord recordCenter = sampleLightSourceCenter(light, its.position, sampler);
    EmitterRecord record = sampleLightSource(light, its.position, sampler);

    bool occluded = isLightOccluded(its, record);

    if (occluded) {
        return 0.0;
    }

    return length(getUnoccludedLightContribution(its, material, recordCenter));
}

bool isLightOccludedCenter(in uint lightIndex, in IntersectionRecord its, inout Sampler sampler) {
    LightSource light = lightBuffer[lightIndex];
    EmitterRecord emitterRecord = sampleLightSourceCenter(light, its.position, sampler);
    bool occluded = isLightOccluded(its, emitterRecord);

    return  occluded;
}

Reservoir CombineReservoirs(in Reservoir reservoir1, in Reservoir reservoir2, in IntersectionRecord its, in MaterialRecord material, inout Sampler sampler)
{
    Reservoir reservoir = Reservoir(0, 0.0, 0.0, 0);

    float pHat1 = unoccludedLightRadianceCenter(reservoir1.lightSelected, its, material, sampler);
    // this simplifies to w_sum if the intersection point is identical (lightWeight = 1/pHat * wsum / M) (not the case with temporal or spatial reuse)
    float w1 = pHat1 * reservoir1.lightWeight * float(reservoir1.M);
    if(IsReservoirValid(reservoir1)){
        UpdateReservoir(reservoir, reservoir1.lightSelected, w1, reservoir1.M, sampler);
    }
    

    float pHat2 = unoccludedLightRadianceCenter(reservoir2.lightSelected, its, material, sampler);
    float w2 = pHat2 * reservoir2.lightWeight * float(reservoir2.M);
    if(IsReservoirValid(reservoir2)){
        UpdateReservoir(reservoir, reservoir2.lightSelected, w2, reservoir2.M, sampler);
    }   

    float pHat = unoccludedLightRadianceCenter(reservoir.lightSelected, its, material, sampler);
    if (pHat <= 0.0 || reservoir.M == 0) {
        reservoir.lightWeight = 0.0;
    } else {
        reservoir.lightWeight = (1.0 / max(pHat, 0.001)) * (reservoir.W_sum / float(reservoir.M));
    }

    return reservoir;
}