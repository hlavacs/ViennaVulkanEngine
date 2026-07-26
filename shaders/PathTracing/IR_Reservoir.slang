struct ReservoirVPL
{
    uint vplIndex; // index of most important vpl
    float lightWeight;            // light weight
    float W_sum; // sum of all weights for all lights processed
    uint M; // number of lights processed for this reservoir
};

ReservoirVPL createEmptyReservoirVPL() {
    return ReservoirVPL(0, 0.0, 0.0, 0);
}

ReservoirVPL readReservoirVPL(in uint2 launchID, int2 launchSize, in RWStructuredBuffer<ReservoirVPL> reservoirBuffer) {
    ReservoirVPL reservoir = reservoirBuffer[launchID.y * launchSize.x + launchID.x];

    return reservoir;
}

void storeReservoirVPL(in ReservoirVPL reservoir, in uint2 launchID, int2 launchSize, in RWStructuredBuffer<ReservoirVPL> reservoirBuffer) {
    reservoirBuffer[launchID.y * launchSize.x + launchID.x] = reservoir;
}