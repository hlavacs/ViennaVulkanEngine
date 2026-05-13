struct ReservoirLVC
{
    uint LVCIndex; // index of most important light
    float lightWeight;            // light weight
    float W_sum; // sum of all weights for all lights processed
    uint M; // number of lights processed for this reservoir
};

ReservoirLVC createEmptyReservoirLVC() {
    return ReservoirLVC(0, 0.0, 0.0, 0);
}

ReservoirLVC readReservoirLVC(in uint2 launchID, int2 launchSize, in RWStructuredBuffer<ReservoirLVC> reservoirBuffer) {
    ReservoirLVC reservoir = reservoirBuffer[launchID.y * launchSize.x + launchID.x];

    return reservoir;
}

void storeReservoirLVC(in ReservoirLVC reservoir, in uint2 launchID, int2 launchSize, in RWStructuredBuffer<ReservoirLVC> reservoirBuffer) {
    reservoirBuffer[launchID.y * launchSize.x + launchID.x] = reservoir;
}