import IR_Defines;

// Helper to separate bits by 2 spaces (for 10-bit numbers)
uint expandBits(uint v) {
    v = (v * 0x00010001u) & 0xFF0000FFu;
    v = (v * 0x00000101u) & 0x0F00F00Fu;
    v = (v * 0x00000011u) & 0xC30C30C3u;
    v = (v * 0x00000005u) & 0x49249249u;
    return v;
}

// Calculates a 30-bit Morton code from a [0.0, 1.0] normalized 3D position
uint calculateMortonCode3D(float3 normalizedPos) {
    // Quantize normalized position to 10 bits (0 to 1023)
    uint x = clamp(uint(normalizedPos.x * 1024.0f), 0, 1023);
    uint y = clamp(uint(normalizedPos.y * 1024.0f), 0, 1023);
    uint z = clamp(uint(normalizedPos.z * 1024.0f), 0, 1023);

    // Interleave bits: x gets bits 2, 5, 8... y gets 1, 4, 7... z gets 0, 3, 6...
    return (expandBits(x) << 2) | (expandBits(y) << 1) | expandBits(z);
}

[[vk::binding(0, 0)]]
StructuredBuffer<VPLReconnectionInfo> vplCache;

[[vk::binding(0, 1)]]
RWStructuredBuffer<uint> mortonCodes;

// You will need to pass your scene bounds to normalize the world positions.
// Adjust the binding/register to match your application.
[[vk::binding(0, 2)]]
cbuffer SceneBounds {
    float3 sceneMin;
    float3 sceneMax;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void main(uint3 threadId: SV_DispatchThreadID) {
    int2 launchID = threadId.xy;
    
    // Optional but recommended: Add a bounds check against your actual VPL count
    // uint totalVPLs;
    // vplCache.GetDimensions(totalVPLs);
    // if (launchID.x >= totalVPLs) return;

    VPLReconnectionInfo vpl = vplCache[launchID.x];

    // 1. Normalize the world position to a [0, 1] range using scene bounding box
    float3 extent = sceneMax - sceneMin;
    float3 normalizedPos = (vpl.position.xyz - sceneMin) / max(extent, 0.0001f);
    
    // 2. Calculate the Morton code
    uint mCode = calculateMortonCode3D(normalizedPos);
    
    // 3. Write out to the buffer
    mortonCodes[launchID.x] = mCode;
}