import Random;
import raytracing;
import Lightsource;
import Surface;
import Materials;
import BSDF;
import Reservoir;
import Bidrirectional_Raytracing;

static const uint CANDIDATE_COUNT_DI = 16;
static const uint CANDIDATE_COUNT_GI = 16;
static const uint MAX_TEMPORAL_INFLUENCE = 20;

static const float DI_WEIGHT = (CANDIDATE_COUNT_DI + CANDIDATE_COUNT_GI) / CANDIDATE_COUNT_DI;
static const float GI_WEIGHT = (CANDIDATE_COUNT_DI + CANDIDATE_COUNT_GI) / CANDIDATE_COUNT_GI;

static const uint MAX_SPATIAL_INFLUENCE = 10;
static const uint SPATIAL_SAMPLES = 24;
static const uint SAMPLE_RADIUS = 12;

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

void clampReadID(inout int2 readID, in int2 launchSize) {
    if (readID.x < 0) {
        readID.x = 0;
    }
    if (readID.y < 0) {
        readID.y = 0;
    }
    if (readID.x > launchSize.x - 1) {
        readID.x = launchSize.x - 1;
    }
    if (readID.y > launchSize.y - 1) {
        readID.y = launchSize.y - 1;
    }
}

bool IsReservoirValid(Reservoir reservoir) {
    return reservoir.M > 0 && reservoir.W_sum > 0;
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

bool isReservoirLVC(Reservoir reservoir) {
    return reservoir.lightSelected >= lightCount;
}

float unoccludedRadiance(in Reservoir reservoir, in IntersectionRecord its, in MaterialRecord material) {
    if (isReservoirLVC(reservoir)) {
        return length(getUnoccludedLightContributionFromConnection(its, material, reservoir.lightSelected - lightCount)) * DI_WEIGHT;
    } else {
        return revaluateUnoccludedLightRadiance(reservoir, its, material) * GI_WEIGHT;
    }
}

float3 occludedRadiance(in Reservoir reservoir, in IntersectionRecord its, in MaterialRecord material) {
    if (isReservoirLVC(reservoir)) {
        return getOccludedLightContributionFromConnection(its, material, reservoir.lightSelected - lightCount) * DI_WEIGHT;
    } else {
        return revaluateOccludedLightRadiance(reservoir, its, material) * GI_WEIGHT;
    }
}

Reservoir CombineReservoirs(in Reservoir reservoir1, in Reservoir reservoir2, in IntersectionRecord its, in MaterialRecord material, inout Sampler sampler)
{
    Reservoir reservoir = createEmptyReservoir();

    float pHat1 = unoccludedRadiance(reservoir1, its, material);
    // this simplifies to w_sum if the intersection point is identical (lightWeight = 1/pHat * wsum / M) (not the case with temporal or spatial reuse)
    float w1 = pHat1 * reservoir1.lightWeight * float(reservoir1.M);
    if(IsReservoirValid(reservoir1)){
        UpdateReservoir(reservoir, reservoir1.lightSelected, reservoir1.samplepoint, w1, reservoir1.M, sampler);
    }

    float pHat2 = unoccludedRadiance(reservoir2, its, material);
    float w2 = pHat2 * reservoir2.lightWeight * float(reservoir2.M);
    if (IsReservoirValid(reservoir2)) {
        UpdateReservoir(reservoir, reservoir2.lightSelected, reservoir2.samplepoint, w2, reservoir2.M, sampler);
    }

    float pHat = unoccludedRadiance(reservoir, its, material);
    if (pHat <= 0.0 || reservoir.M == 0) {
        reservoir.lightWeight = 0.0;
    } else {
        reservoir.lightWeight = (1.0 / max(pHat, 0.001)) * (reservoir.W_sum / float(reservoir.M));
    }

    return reservoir;
}

Reservoir CombineReservoirsOccluded(in Reservoir reservoir1, in Reservoir reservoir2, in IntersectionRecord its, in MaterialRecord material, inout Sampler sampler)
{
    Reservoir reservoir = createEmptyReservoir();

    float pHat1 = length(occludedRadiance(reservoir1, its, material));
    // this simplifies to w_sum if the intersection point is identical (lightWeight = 1/pHat * wsum / M) (not the case with temporal or spatial reuse)
    float w1 = pHat1 * reservoir1.lightWeight * float(reservoir1.M);
    if (IsReservoirValid(reservoir1)) {
        UpdateReservoir(reservoir, reservoir1.lightSelected, reservoir1.samplepoint, w1, reservoir1.M, sampler);
    }

    float pHat2 = length(occludedRadiance(reservoir2, its, material));
    float w2 = pHat2 * reservoir2.lightWeight * float(reservoir2.M);
    if (IsReservoirValid(reservoir2)) {
        UpdateReservoir(reservoir, reservoir2.lightSelected, reservoir2.samplepoint, w2, reservoir2.M, sampler);
    }

    float pHat = length(occludedRadiance(reservoir, its, material));
    if (pHat <= 0.0 || reservoir.M == 0) {
        reservoir.lightWeight = 0.0;
    } else {
        reservoir.lightWeight = (1.0 / max(pHat, 0.001)) * (reservoir.W_sum / float(reservoir.M));
    }

    return reservoir;
}