import Random;
import raytracing;
import Lightsource;
import Surface;
import Materials;
import BSDF;

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

struct ReservoirGI {
    float3 position;
    float3 normal;
    float3 radiance;
    float W_sum;
    uint M;
    uint age;
}

ReservoirGI createEmptyReservoirGI() {
    return ReservoirGI(float3(0.0), float3(0.0, 0.0, 1.0), float3(0.0), 0.0, 0, 0);
}


bool UpdateReservoirGI(inout ReservoirGI reservoir, in ReservoirGI reservoirNew, float w, inout Sampler sampler)
{
    // 1. Sanity check for NaN/Inf
    if (isnan(w) || isinf(w)) w = 0.0;

    reservoir.W_sum += w;
    reservoir.M += reservoirNew.M;

    if (sampler.UniformFloat() < (w / reservoir.W_sum))
    {
        reservoir.position = reservoirNew.position;
        reservoir.normal = reservoirNew.normal;
        reservoir.radiance = reservoirNew.radiance;
        reservoir.age = reservoirNew.age;
        return true;
    }

    return false;
}

bool IsReservoirValidGI(ReservoirGI reservoir) {
    return reservoir.M > 0 && reservoir.W_sum > 0;
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

//GI

EmitterRecord getEmmiterRecordFromReservoir(ReservoirGI reservoir, IntersectionRecord its) {
    EmitterRecord record;

    float3 toLight = reservoir.position.xyz - its.position.xyz;

    // not needed
    record.intersection = float3(0.0);
    record.distance = 0.0;

    record.direction = normalize(toLight);
    // this is inaccurate which is maybe why one needs the jacobian. It would be possible to do this with a brdf evaluation at the secundary intersection, but one would have to store too much.
    record.sampleEmission = reservoir.radiance;
}

float getApproximateRadianceFromReservoir(in ReservoirGI reservoir, in IntersectionRecord its, in MaterialRecord material) {
    EmitterRecord record = getEmmiterRecordFromReservoir(reservoir, its);
    // maybe I need to devide by reservoir.M here
    return length(getUnoccludedLightContribution(its, material, record));
}

ReservoirGI CombineReservoirsGI(in ReservoirGI reservoir1, in ReservoirGI reservoir2, in IntersectionRecord its, in MaterialRecord material, inout Sampler sampler)
{
    float pHat2 = getApproximateRadianceFromReservoir(reservoir2, its, material);
    float w2 = pHat2 * reservoir2.W_sum * float(reservoir2.M);
    if (IsReservoirValidGI(reservoir2)) {
        UpdateReservoirGI(reservoir1, reservoir2, w2, sampler);
    }

    return reservoir1;
}

ReservoirGI createInitialReservoirGI(in IntersectionRecord its, in MaterialRecord materialRecord, inout Sampler sampler) {

    ReservoirGI reservoir = createEmptyReservoirGI();

    BSDFQueryRecord shaderRecord = constructBSDFRecord(its, materialRecord, false);

    float3 bsdf = samplePrincipled(shaderRecord, materialRecord, sampler);
    RayDesc indirectRay = getRayInternal(its, shaderRecord.woWorld);

    HitPayload payload;
    const uint rayFlags = 0;
    TraceRay(topLevelAS, rayFlags, 0xff, 0, 0, 0, indirectRay, payload);

    if (!payload.hit) {
        return reservoir;
    }

    IntersectionRecord its_secondary = getIntersectionInfo(payload, indirectRay);

    Material material_secondary = materials[its_secondary.materialIdx];
    MaterialRecord materialRecord_secondary = getMaterialProperties(material_secondary, textures, its_secondary.uv, its_secondary.frame);

    float3 directLight_secondary = directRaycast(its_secondary, materialRecord_secondary, sampler);

    reservoir.position = its_secondary.position;
    reservoir.normal = its_secondary.normal;
    reservoir.M = 1;
    //posibly incorrect
    reservoir.radiance = directLight_secondary;
    //most likley incorrect
    reservoir.W_sum = length(bsdf * directLight_secondary);

    return reservoir;
}