import IR_Reservoir;
import Random;
import IR_VPL_Evaluation;
import Surface;
import Materials;
import IR_Defines;
import IR_Bindings;

bool UpdateReservoirVPL(inout ReservoirVPL reservoir, uint vplIndex, float w, uint count, inout Sampler sampler)
{

    reservoir.M += count;

    // 1. Sanity check for NaN/Inf
    if (isnan(w) || isinf(w) || w <= 0.0) {
        return false;
    } else {
        reservoir.W_sum += w;
        if (sampler.UniformFloat() < (w / reservoir.W_sum))
        {
            reservoir.vplIndex = vplIndex;
            return true;
        }

        return false;
    }
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

bool IsReservoirValidVPL(ReservoirVPL reservoir) {
    return reservoir.M > 0 && reservoir.W_sum > 0;
}

// this function uses waaaaaayy to many vpl load operations from the buffer. Reduce this by passing from the outside
ReservoirVPL CombineReservoirsVPL(in ReservoirVPL reservoir1, in ReservoirVPL reservoir2, in VPLReconnectionInfo vpl1, in VPLReconnectionInfo vpl2, in IntersectionRecord its, in MaterialRecord material, inout Sampler sampler, out VPLReconnectionInfo vpl)
{
    ReservoirVPL reservoir = createEmptyReservoirVPL();

    float pHat1 = length(getUnoccludedContributionFromVPL(its, material, vpl1));
    // this simplifies to w_sum if the intersection point is identical (lightWeight = 1/pHat * wsum / M) (not the case with temporal or spatial reuse)
    float w1 = pHat1 * reservoir1.lightWeight * float(reservoir1.M);
    if (IsReservoirValidVPL(reservoir1)) {
        if (UpdateReservoirVPL(reservoir, reservoir1.vplIndex, w1, reservoir1.M, sampler)) {
            vpl = vpl1;
        }
    }

    float pHat2 = length(getUnoccludedContributionFromVPL(its, material, vpl2));
    float w2 = pHat2 * reservoir2.lightWeight * float(reservoir2.M);
    if (IsReservoirValidVPL(reservoir2)) {
        if (UpdateReservoirVPL(reservoir, reservoir2.vplIndex, w2, reservoir2.M, sampler)) {
            vpl = vpl2;
        }
    }

    float pHat = length(getUnoccludedContributionFromVPL(its, material, vpl));
    if (pHat <= 0.0 || reservoir.M == 0) {
        reservoir.lightWeight = 0.0;
    } else {
        reservoir.lightWeight = (1.0 / max(pHat, 0.001)) * (reservoir.W_sum / float(reservoir.M));
    }

    return reservoir;
}