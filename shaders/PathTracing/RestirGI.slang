import Random;
import raytracing;
import Lightsource;
import Surface;
import Materials;
import BSDF;
import ReservoirGI;

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

EmitterRecord getEmmitterRecordFromReservoir(ReservoirGI reservoir, IntersectionRecord its) {
    EmitterRecord record;

    float3 toLight = reservoir.position.xyz - its.position.xyz;

    // not needed
    record.intersection = reservoir.position.xyz;
    //needed for efficient visibility check!
    record.distance = length(toLight);

    record.direction = normalize(toLight);
    // this is inaccurate which is maybe why one needs the jacobian. It would be possible to do this with a brdf evaluation at the secundary intersection, but one would have to store too much.
    record.sampleEmission = reservoir.radiance;
}

float getApproximateRadianceFromReservoir(in ReservoirGI reservoir, in IntersectionRecord its, in MaterialRecord material) {
    EmitterRecord record = getEmmitterRecordFromReservoir(reservoir, its);
    // maybe I need to devide by reservoir.M here
    return length(getUnoccludedLightContribution(its, material, record));
}

float3 getGroundTruthRadianceFromReservoir(in ReservoirGI reservoir, in IntersectionRecord its, in MaterialRecord material) {
    EmitterRecord record = getEmmitterRecordFromReservoir(reservoir, its);

    //This check is incorrect, it will alwys return occluded!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    bool occluded = isLightOccluded(its, record);

    if (occluded) {
        return float3(0.0, 0.0, 0.0);
    }

    return getUnoccludedLightContribution(its, material, record);
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
    reservoir.radiance = directLight_secondary;
    // why reservoir.W_sum = 1.0 / shaderRecord.pdf; and not bsdf * directLight_secondary?
    // later we combine this reservoir with a empty main reservoir.
    // during combination we do bsdfEval(lacking pdf) * radiance * wsum (including pdf) * M
    // this is identical to doing bsdf * directLight_secondary
    reservoir.W_sum = 1.0 / shaderRecord.pdf;

    return reservoir;
}

/*
float3 evaluateReservoirGI(ReservoirGI reservoir, in IntersectionRecord its, in MaterialRecord materialRecord) {
    float3 unnormalizedRadiance = getGroundTruthRadianceFromReservoir(reservoir, its, materialRecord);
    float weight = length(reservoir.radiance) * reservoir.W_sum * float(reservoir.M);
    float3 radiance = unnormalizedRadiance * weight;
    return radiance;
}
*/

float3 evaluateReservoirGI(ReservoirGI reservoir, in IntersectionRecord its, in MaterialRecord materialRecord) {
    if (!IsReservoirValidGI(reservoir)) return float3(0.0);

    // 1. Get the actual unshadowed contribution (BSDF * Radiance)
    float3 f_times_L = getGroundTruthRadianceFromReservoir(reservoir, its, materialRecord);

    // 2. Re-calculate pHat for the "winner" at the current pixel
    float pHat = length(f_times_L);

    // 3. Calculate the normalization factor W
    // W = (1 / pHat) * (W_sum / M)
    float W = 0.0;
    if (pHat > 0.0 && reservoir.M > 0) {
        W = reservoir.W_sum / (pHat * float(reservoir.M));
    }

    return f_times_L * W;
}

void getPartialJacobian(const float3 RecieverPos, const float3 SamplePos, const float3 SampleNormal, out float DistanceToSurfaceSqr, out float CosineEmissionAngle)
{
    const float3 Vec = RecieverPos - SamplePos;

    DistanceToSurfaceSqr = dot(Vec, Vec);
    CosineEmissionAngle = saturate(dot(SampleNormal, Vec * rsqrt(DistanceToSurfaceSqr)));
}

float getJacobian(float3 recieverPos, float3 neighborReceiverPos, float3 neighborPosition, float3 neighborNormal)
{
    float OriginalDistanceSqr = 0.0;
    float OriginalCosine = 0.0;
    float NewDistanceSqr = 0.0;
    float NewCosine = 0.0;
    getPartialJacobian(recieverPos, neighborPosition, neighborNormal, NewDistanceSqr, NewCosine);
    getPartialJacobian(neighborReceiverPos, neighborPosition, neighborNormal, OriginalDistanceSqr, OriginalCosine);

    float jacobian = (NewCosine * OriginalDistanceSqr) / (OriginalCosine * NewDistanceSqr);

    if (isinf(jacobian) || isnan(jacobian))
        jacobian = 0.0;

    return jacobian;
}