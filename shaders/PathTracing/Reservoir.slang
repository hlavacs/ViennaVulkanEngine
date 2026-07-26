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

// Keep your existing bit interleave function
uint bitInterleave(uint x) {
    x = (x | (x << 8)) & 0x00FF00FF;
    x = (x | (x << 4)) & 0x0F0F0F0F;
    x = (x | (x << 2)) & 0x33333333;
    x = (x | (x << 1)) & 0x55555555;
    return x;
}

// Morton code only needs to handle 0-7 now (8x8 tile)
uint getMortonIndexWithinTile(uint2 localID) {
    return bitInterleave(localID.x) | (bitInterleave(localID.y) << 1);
}

uint getIndex(in uint2 launchID, in int2 launchSize) {
    uint2 tileSize = uint2(8, 8);

    // 1. Which tile are we in?
    uint2 tileID = launchID / tileSize;

    // 2. Where are we inside the 8x8 tile?
    uint2 localID = launchID % tileSize;

    // 3. How many tiles fit across the screen horizontally?
    uint numTilesX = (launchSize.x + tileSize.x - 1) / tileSize.x;

    // 4. Calculate the global starting index for this specific tile
    uint tileIndex = tileID.y * numTilesX + tileID.x;

    // 5. Calculate the Z-curve index (0 to 63) inside the tile
    uint localMortonIndex = getMortonIndexWithinTile(localID);

    // Combine tile offset with local Morton offset
    return (tileIndex * 64) + localMortonIndex;
}

Reservoir readReservoir(in uint2 launchID, int2 launchSize, in RWStructuredBuffer<Reservoir> reservoirBuffer) {
    Reservoir reservoir = reservoirBuffer[getIndex(launchID, launchSize)];

    return reservoir;
}

void storeReservoir(in Reservoir reservoir, in uint2 launchID, int2 launchSize, in RWStructuredBuffer<Reservoir> reservoirBuffer) {
    reservoirBuffer[getIndex(launchID, launchSize)] = reservoir;
}