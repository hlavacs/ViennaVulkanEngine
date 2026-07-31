import IR_Defines;
import Defines;

[[vk::binding(0, 0)]]
RWStructuredBuffer<VPLReconnectionInfo> vplCache;

[[vk::binding(1, 0)]]
StructuredBuffer<VPLShading> vplRadianceCache;

[[vk::binding(2, 0)]] StructuredBuffer<uint> sortedIndices;

[[vk::binding(3, 0)]]
cbuffer UniformBufferBidirectional
{
    uint vplCacheSize;
    float3 AABBMin;
    float3 AABBMax;
};

// --- kNN Constants ---
// Maximum K allowed so we can allocate a fixed-size array in registers.
// Dynamic array sizes in HLSL spill to slow local memory.
static const uint MAX_K = 16;
// How many elements to check left and right in the Morton array.
// Higher = more accurate but slower. 64-128 is a common sweet spot.
static const uint SEARCH_WINDOW = 64;

// Helper function to insert a new distance into our sorted top-K array
void InsertDistance(inout float minDistSq[MAX_K], uint k, float distSq) {
    // Early out if it's further than our current k-th nearest
    if (distSq >= minDistSq[k - 1]) return;

    int insertPos = k - 1;

    // Shift elements down to make room (written to safely unroll on GPU)
    for (int j = MAX_K - 1; j > 0; --j) {
        if (j <= insertPos && distSq < minDistSq[j - 1]) {
            minDistSq[j] = minDistSq[j - 1];
            insertPos = j - 1;
        }
    }
    minDistSq[insertPos] = distSq;
}

[shader("compute")]
[numthreads(256, 1, 1)]
void main(uint3 threadId: SV_DispatchThreadID) {
    uint launchID = threadId.x;

    // Bounds check
    if (launchID >= vplCacheSize) return;

    uint vplIndex = sortedIndices[launchID];
    VPLReconnectionInfo vpl = vplCache[vplIndex];
    VPLShading vplRadiance = vplRadianceCache[vplIndex];

    // Calculate k based on total VPL count
    uint k = (uint)max(1.0f, sqrt((float)vplCacheSize) / 32.0f);

    // Clamp k to prevent out-of-bounds indexing in our local array
    k = clamp(k, 1, MAX_K);

    // Initialize local distances to "infinity"
    float minDistSq[MAX_K];
    for (uint j = 0; j < MAX_K; ++j) {
        minDistSq[j] = INFINITE;
    }

    // --- Windowed kNN Search ---
    for (uint i = 1; i <= SEARCH_WINDOW; ++i) {
        // Search Left
        if (launchID >= i) {
            uint leftVplIndex = sortedIndices[launchID - i];
            float3 diff = vplCache[leftVplIndex].position - vpl.position;
            float distSq = dot(diff, diff);
            InsertDistance(minDistSq, k, distSq);
        }

        // Search Right
        if (launchID + i < vplCacheSize) {
            uint rightVplIndex = sortedIndices[launchID + i];
            float3 diff = vplCache[rightVplIndex].position - vpl.position;
            float distSq = dot(diff, diff);
            InsertDistance(minDistSq, k, distSq);
        }
    }

    // --- PDF Calculation ---
    // Extract the distance squared to the k-th nearest neighbor
    /*
    float rk_sq = minDistSq[k - 1];

    // Prevent division by zero if VPLs are exactly overlapping
    rk_sq = max(rk_sq, 1e-8f);

    // Formula: p(y) = k / (I * Area) = k / (I * pi * r_k^2)
    float pdf = (float)k / ((float)vplCacheSize * M_PI * rk_sq);

    */

    float avgDistSq = 0.0f;

    for (uint i = 0; i < k; ++i)
        avgDistSq += minDistSq[i];

    avgDistSq /= k;

    float pdf = (float)k / ((float)vplCacheSize * M_PI * avgDistSq);

    vplCache[vplIndex].weightedRadiance = vplRadiance.radiance / pdf;

    //vplCache[vplIndex].weightedRadiance = vplRadiance.radiance * 1000.0;
}