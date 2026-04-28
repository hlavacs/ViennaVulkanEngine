struct Reservoir
{
    float4 samplepoint; // position sampled on the light source should be changed to uv coords for moving light sources!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    uint lightSelected; // index of most important light
    float lightWeight;            // light weight
    float W_sum; // sum of all weights for all lights processed
    uint M; // number of lights processed for this reservoir
};

Reservoir createEmptyReservoir() {
    return Reservoir(float4(0.0), 0, 0.0, 0.0, 0);
}

Reservoir readReservoir(in uint2 launchID, int2 launchSize, in RWStructuredBuffer<Reservoir> reservoirBuffer) {
    Reservoir reservoir = reservoirBuffer[launchID.y * launchSize.x + launchID.x];

    return reservoir;
}

void storeReservoir(in Reservoir reservoir, in uint2 launchID, int2 launchSize, in RWStructuredBuffer<Reservoir> reservoirBuffer) {
    reservoirBuffer[launchID.y * launchSize.x + launchID.x] = reservoir;
}