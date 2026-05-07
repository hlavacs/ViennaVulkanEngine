struct ReservoirGI {
    float3 position;
    float3 normal;
    float3 radiance;
    float W_sum;
    uint M;
    uint age;
    float lightWeight;
}

ReservoirGI createEmptyReservoirGI() {
    return ReservoirGI(float3(0.0), float3(0.0, 0.0, 1.0), float3(0.0), 0.0, 0, 0, 0.0);
}

ReservoirGI readReservoirGI(in uint2 launchID, int2 launchSize, in RWStructuredBuffer<ReservoirGI> reservoirGIBuffer) {
    ReservoirGI reservoir = reservoirGIBuffer[launchID.y * launchSize.x + launchID.x];

    return reservoir;
}

void storeReservoirGI(in ReservoirGI reservoir, in uint2 launchID, int2 launchSize, in RWStructuredBuffer<ReservoirGI> reservoirGIBuffer) {
    reservoirGIBuffer[launchID.y * launchSize.x + launchID.x] = reservoir;
}