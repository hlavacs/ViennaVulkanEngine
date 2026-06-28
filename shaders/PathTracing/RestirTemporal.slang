import Defines;
import Materials;
import Lightsource;
import Random;
import Frame;
import BSDF;
import ColorSpace;
import Surface;
import raytracing;
import Reservoir;
import RestirDI;

void storeEmptyReservoir(in int2 launchID, in int2 launchSize, in RWStructuredBuffer<Reservoir> outReservoir) {
    Reservoir reservoir = createEmptyReservoir();
    storeReservoir(reservoir, launchID, launchSize, outReservoir);
}

void restirTemporalDI(in int2 launchID, in int2 launchSize, inout Sampler sampler, in MaterialRecord material, in IntersectionRecord its,
                      in RWTexture2D<float4> inReprojectionError, in RWStructuredBuffer<Reservoir> inReservoir, in RWStructuredBuffer<Reservoir> outReservoir)
{
    const uint CANDIDATE_COUNT = candidateSamplesRestirDi;
    const uint MAX_TEMPORAL_INFLUENCE = 20;

    float lightPdf = 1.0 / float(lightCount);
    float p_hat = 0;

    Reservoir reservoir = createEmptyReservoir();

    // initial selection of 1 light of M
    for (uint i = 0; i < CANDIDATE_COUNT; i++)
    {
        uint lightIndex = sampler.UniformUIntFast(0, lightCount - 1);

        float4 samplePosition = float4(0.0);

        p_hat = sampleUnoccludedLightRadiance(lightIndex, its, material, sampler, samplePosition);

        float w = p_hat / lightPdf;

        UpdateReservoir(reservoir, lightIndex, samplePosition, w, 1, sampler);
    }

    if (IsReservoirValid(reservoir))
    {
        // Update weight with new radiance with visibility term!
        float p_hat = length(revaluateOccludedLightRadiance(reservoir, its, material));
        reservoir.lightWeight = p_hat > 0.0 ? (1.0 / max(p_hat, 0.001)) * reservoir.W_sum / reservoir.M : 0.0;
    } else {
        reservoir = createEmptyReservoir();
    }

    //reject history if reprojection error is too big
    //better system needed in the future
    if (inReprojectionError[launchID].x < 0.5) {
        Reservoir temporalReservoir = readReservoir(launchID, launchSize, inReservoir);

        // restrict influence from past samples.
        // this may not be optimal, the real time raytracing engine handles this diffrently
        temporalReservoir.M = min(MAX_TEMPORAL_INFLUENCE * reservoir.M, temporalReservoir.M);

        reservoir = CombineReservoirs(reservoir, temporalReservoir, its, material, sampler);
    }

    storeReservoir(reservoir, launchID, launchSize, outReservoir);  
}

