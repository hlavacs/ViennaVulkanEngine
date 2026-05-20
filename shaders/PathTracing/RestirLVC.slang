import Random;
import raytracing;
import Lightsource;
import Surface;
import Materials;
import BSDF;
import ReservoirLVC;
import Bidrirectional_Raytracing;
import LightVertex;

bool UpdateReservoirLVC(inout ReservoirLVC reservoir, uint LVCIndex, float w, uint count, inout Sampler sampler)
{

    reservoir.M += count;

    // 1. Sanity check for NaN/Inf
    if (isnan(w) || isinf(w) || w <= 0.0) {
        return false;
    } else {
        reservoir.W_sum += w;
        if (sampler.UniformFloat() < (w / reservoir.W_sum))
        {
            reservoir.LVCIndex = LVCIndex;
            return true;
        }

        return false;
    }



}

bool IsReservoirValidLVC(ReservoirLVC reservoir) {
    return reservoir.M > 0 && reservoir.W_sum > 0;
}

ReservoirLVC CombineReservoirsLVC(in ReservoirLVC reservoir1, in ReservoirLVC reservoir2, in IntersectionRecord its, in MaterialRecord material, inout Sampler sampler)
{
    ReservoirLVC reservoir = createEmptyReservoirLVC();

    float pHat1 = length(getUnoccludedLightContributionFromConnection(its, material, reservoir1.LVCIndex));
    // this simplifies to w_sum if the intersection point is identical (lightWeight = 1/pHat * wsum / M) (not the case with temporal or spatial reuse)
    float w1 = pHat1 * reservoir1.lightWeight * float(reservoir1.M);
    if(IsReservoirValidLVC(reservoir1)){
        UpdateReservoirLVC(reservoir, reservoir1.LVCIndex, w1, reservoir1.M, sampler);
    }

    float pHat2 = length(getUnoccludedLightContributionFromConnection(its, material, reservoir2.LVCIndex));
    float w2 = pHat2 * reservoir2.lightWeight * float(reservoir2.M);
    if (IsReservoirValidLVC(reservoir2)) {
        UpdateReservoirLVC(reservoir, reservoir2.LVCIndex, w2, reservoir2.M, sampler);
    }

    float pHat = length(getUnoccludedLightContributionFromConnection(its, material, reservoir.LVCIndex));
    if (pHat <= 0.0 || reservoir.M == 0) {
        reservoir.lightWeight = 0.0;
    } else {
        reservoir.lightWeight = (1.0 / max(pHat, 0.001)) * (reservoir.W_sum / float(reservoir.M));
    }

    return reservoir;
}