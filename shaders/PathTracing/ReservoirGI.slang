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
    // 1. Set your tile size here (e.g., 4, 8, 16)
    static const uint TILE_DIM = 16;
    static const uint PIXELS_PER_TILE = TILE_DIM * TILE_DIM;

    // 2. Tile and local coordinates
    uint2 tileID = launchID / TILE_DIM;
    uint2 localID = launchID % TILE_DIM;

    // 3. Number of tiles across the screen
    uint numTilesX = (launchSize.x + TILE_DIM - 1) / TILE_DIM;

    // 4. Global tile offset + local Morton offset
    uint tileIndex = tileID.y * numTilesX + tileID.x;
    uint localMorton = bitInterleave(localID.x) | (bitInterleave(localID.y) << 1);

    return (tileIndex * PIXELS_PER_TILE) + localMorton;
}

ReservoirGI readReservoirGI(in uint2 launchID, in int2 launchSize, in RWStructuredBuffer<ReservoirGI> reservoirGIBuffer) {
    ReservoirGI reservoir = reservoirGIBuffer[getIndex(launchID, launchSize)];

    return reservoir;
}

void storeReservoirGI(in ReservoirGI reservoir, in uint2 launchID, in int2 launchSize, in RWStructuredBuffer<ReservoirGI> reservoirGIBuffer) {
    reservoirGIBuffer[getIndex(launchID, launchSize)] = reservoir;
}