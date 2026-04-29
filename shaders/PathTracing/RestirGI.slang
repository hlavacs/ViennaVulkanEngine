import Random;
import raytracing;
import Lightsource;
import Surface;
import Materials;
import BSDF;
import ReservoirGI;
import Defines;

bool IsReservoirValidGI(ReservoirGI reservoir) {
    return reservoir.M > 0 && reservoir.W_sum > 0 && !isinf(reservoir.W_sum) && !isnan(reservoir.W_sum) && !any(isinf(reservoir.radiance)) && !any(isnan(reservoir.radiance));
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

    return record;
}

float getApproximateRadianceFromReservoir(in ReservoirGI reservoir, in IntersectionRecord its, in MaterialRecord material) {
    EmitterRecord record = getEmmitterRecordFromReservoir(reservoir, its);
    // maybe I need to devide by reservoir.M here
    return length(getUnoccludedLightContribution(its, material, record));
}

//check for correctness
bool isReservoirOccluded(in IntersectionRecord its, EmitterRecord emitterRecord) {
    RayDesc shadowRay = getRay(its, emitterRecord.direction);
    shadowRay.TMax = emitterRecord.distance;
    HitPayload payload;
    const uint rayFlags = 0;

    // self blocked
    if (dot(shadowRay.Direction, its.normal) < 0.0) {
        return true;
    }

    TraceRay(topLevelAS, rayFlags, 0xff, 0, 0, 0, shadowRay, payload);

    if (payload.distance < emitterRecord.distance + EPSILON && payload.distance > emitterRecord.distance - EPSILON) {
        return false;
    }

    return true;
}

float3 getGroundTruthRadianceFromReservoir(in ReservoirGI reservoir, in IntersectionRecord its, in MaterialRecord material) {
    EmitterRecord record = getEmmitterRecordFromReservoir(reservoir, its);

    bool occluded = isReservoirOccluded(its, record);

    if (occluded) {
        return float3(0.0, 0.0, 0.0);
    }

    return getUnoccludedLightContribution(its, material, record);
}

void updateLightWeight(inout ReservoirGI reservoir, in IntersectionRecord its, in MaterialRecord material) {
    float p_hat = getApproximateRadianceFromReservoir(reservoir, its, material);
    reservoir.lightWeight = p_hat > 0.0 ? (1.0 / max(p_hat, 0.001)) * reservoir.W_sum / reservoir.M : 0.0;
}

bool CombineReservoirsGI(inout ReservoirGI reservoir, in ReservoirGI reservoirNew, in float targetPdf, inout Sampler sampler)
{
    float w = targetPdf * reservoirNew.W_sum * float(reservoirNew.M);

    // 1. Sanity check for NaN/Inf
    if (isnan(w) || isinf(w)) w = 0.0;

    reservoir.W_sum += w;
    reservoir.M += reservoirNew.M;

    bool selected = sampler.UniformFloat() < (w / reservoir.W_sum);

    if (selected)
    {
        reservoir.position = reservoirNew.position;
        reservoir.normal = reservoirNew.normal;
        reservoir.radiance = reservoirNew.radiance;
        reservoir.age = reservoirNew.age;
        return true;
    }

    return selected;
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

    if (any(isnan(directLight_secondary)) || any(isinf(directLight_secondary))) {
        directLight_secondary = float3(0.0);
    }

    reservoir.radiance = directLight_secondary;
    // why reservoir.W_sum = 1.0 / shaderRecord.pdf; and not bsdf * directLight_secondary?
    // later we combine this reservoir with a empty main reservoir.
    // during combination we do bsdfEval(lacking pdf) * radiance * wsum (including pdf) * M
    // this is identical to doing bsdf * directLight_secondary
    reservoir.W_sum = 1.0 / max(shaderRecord.pdf, 0.001);

    if (any(isnan(reservoir.W_sum)) || any(isinf(reservoir.W_sum))) {
        reservoir.W_sum = 0.0;
    }

    return reservoir;
}

void finalizeGIResampling(
    inout ReservoirGI reservoir,
    float normalizationNumerator,
    float normalizationDenominator)
{
    reservoir.W_sum = (normalizationDenominator == 0.0) ? 0.0 : (reservoir.W_sum * normalizationNumerator) / normalizationDenominator;
}

float3 evaluateReservoirGI(inout ReservoirGI reservoir, in IntersectionRecord its, in MaterialRecord materialRecord) {
    if (!IsReservoirValidGI(reservoir)) return float3(0.0);

    // Get the actual unshadowed contribution (BSDF * Radiance)
    float3 f_times_L = getGroundTruthRadianceFromReservoir(reservoir, its, materialRecord);
    return f_times_L * reservoir.W_sum;
}

void normalizeReservoir(inout ReservoirGI reservoirGI, float finalPHat) {
    if (finalPHat > 0 && reservoirGI.M > 0) {
        reservoirGI.W_sum = reservoirGI.W_sum / (float(reservoirGI.M) * finalPHat);
    } else {
        reservoirGI.W_sum = 0;
    }
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

bool validateJacobian(inout float jacobian)
{
    if (jacobian > 10.0 || jacobian < 1 / 10.0) {
        return false;
    }

    jacobian = clamp(jacobian, 1 / 3.0, 3.0);

    return true;
}