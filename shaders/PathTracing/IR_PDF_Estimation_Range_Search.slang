import IR_Defines;
import Defines;

[[vk::binding(0, 0)]] RWStructuredBuffer<VPLReconnectionInfo> vplCache;
[[vk::binding(1, 0)]] StructuredBuffer<VPLShading> vplRadianceCache;
[[vk::binding(2, 0)]] StructuredBuffer<uint> sortedIndices;
[[vk::binding(3, 0)]] cbuffer UniformBufferBidirectional
{
    uint vplCacheSize;
    float3 AABBMin;
    float3 AABBMax;
};
[[vk::binding(4, 0)]] StructuredBuffer<uint> sortedMortonCodes; 

static const uint MAX_K = 16;
static const uint SEARCH_WINDOW = 64;

// --- Bitwise Morton Helpers ---

uint expandBits(uint v) {
    v = (v * 0x00010001u) & 0xFF0000FFu;
    v = (v * 0x00000101u) & 0x0F00F00Fu;
    v = (v * 0x00000011u) & 0xC30C30C3u;
    v = (v * 0x00000005u) & 0x49249249u;
    return v;
}

uint compactBits(uint v) {
    v &= 0x09249249u;
    v = (v ^ (v >> 2)) & 0x030C30C3u;
    v = (v ^ (v >> 4)) & 0x0300F00Fu;
    v = (v ^ (v >> 8)) & 0xFF0000FFu;
    v = (v ^ (v >> 16)) & 0x000003FFu;
    return v;
}

uint calculateMortonCode3D_uint3(uint3 p) {
    return (expandBits(p.x) << 2) | (expandBits(p.y) << 1) | expandBits(p.z);
}

uint3 decodeMorton3D(uint code) {
    return uint3(compactBits(code >> 2), compactBits(code >> 1), compactBits(code));
}

// --- Binary Search ---

uint LowerBound(uint targetCode, uint minIdx, uint maxIdx) {
    uint low = minIdx;
    uint high = maxIdx;
    while (low < high) {
        uint mid = low + (high - low) / 2;
        if (sortedMortonCodes[mid] < targetCode) low = mid + 1;
        else high = mid;
    }
    return low;
}

// --- Exact Z-Order BIGMIN Algorithm ---

uint GetNextValidMorton(uint code, uint3 minBox, uint3 maxBox) {
    uint3 p = decodeMorton3D(code);
    
    int highestMortonBit = -1;
    bool isAbove = false;
    
    // Check X axis (Morton bits 2, 5, 8, 11, ...)
    if (p.x > maxBox.x) {
        uint diff = p.x & ~maxBox.x;
        int mBit = firstbithigh(diff) * 3 + 2;
        if (mBit > highestMortonBit) { highestMortonBit = mBit; isAbove = true; }
    } else if (p.x < minBox.x) {
        uint diff = minBox.x & ~p.x;
        int mBit = firstbithigh(diff) * 3 + 2;
        if (mBit > highestMortonBit) { highestMortonBit = mBit; isAbove = false; }
    }
    
    // Check Y axis (Morton bits 1, 4, 7, 10, ...)
    if (p.y > maxBox.y) {
        uint diff = p.y & ~maxBox.y;
        int mBit = firstbithigh(diff) * 3 + 1;
        if (mBit > highestMortonBit) { highestMortonBit = mBit; isAbove = true; }
    } else if (p.y < minBox.y) {
        uint diff = minBox.y & ~p.y;
        int mBit = firstbithigh(diff) * 3 + 1;
        if (mBit > highestMortonBit) { highestMortonBit = mBit; isAbove = false; }
    }
    
    // Check Z axis (Morton bits 0, 3, 6, 9, ...)
    if (p.z > maxBox.z) {
        uint diff = p.z & ~maxBox.z;
        int mBit = firstbithigh(diff) * 3 + 0;
        if (mBit > highestMortonBit) { highestMortonBit = mBit; isAbove = true; }
    } else if (p.z < minBox.z) {
        uint diff = minBox.z & ~p.z;
        int mBit = firstbithigh(diff) * 3 + 0;
        if (mBit > highestMortonBit) { highestMortonBit = mBit; isAbove = false; }
    }
    
    // Fallback if no bit violation detected
    if (highestMortonBit == -1) return code + 1;
    
    uint minCode = calculateMortonCode3D_uint3(minBox);
    uint nextCode;
    
    if (isAbove) {
        // Carry 1 into the bit position above the violating MSB
        uint mask = 0xFFFFFFFFu << (highestMortonBit + 1);
        nextCode = ((code & mask) + (1u << (highestMortonBit + 1))) | (minCode & ~mask);
    } else {
        // Set the violating MSB to 1 and fill lower bits with minCode
        uint mask = 0xFFFFFFFFu << highestMortonBit;
        nextCode = (code & mask) | (1u << highestMortonBit) | (minCode & ~mask);
    }
    
    // Ensure monotonic advancement
    return max(nextCode, code + 1);
}

// --- kNN Core ---

void InsertDistance(inout float minDistSq[MAX_K], uint k, float distSq) {
    if (distSq >= minDistSq[k - 1]) return;
    int insertPos = k - 1;
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
    if (launchID >= vplCacheSize) return;

    uint vplIndex = sortedIndices[launchID];
    VPLReconnectionInfo vpl = vplCache[vplIndex];
    VPLShading vplRadiance = vplRadianceCache[vplIndex];

    uint k = clamp((uint)max(1.0f, sqrt((float)vplCacheSize) / 32.0f), 1, MAX_K);

    float minDistSq[MAX_K];
    for (uint j = 0; j < MAX_K; ++j) minDistSq[j] = INFINITE;

    // =========================================================================
    // PASS 1: Fast Local Window Scan (Establishes initial upper-bound radius)
    // =========================================================================
    for (uint i = 1; i <= SEARCH_WINDOW; ++i) {
        if (launchID >= i) {
            uint leftIdx = sortedIndices[launchID - i];
            float3 diff = vplCache[leftIdx].position - vpl.position;
            InsertDistance(minDistSq, k, dot(diff, diff));
        }
        if (launchID + i < vplCacheSize) {
            uint rightIdx = sortedIndices[launchID + i];
            float3 diff = vplCache[rightIdx].position - vpl.position;
            InsertDistance(minDistSq, k, dot(diff, diff));
        }
    }

    // =========================================================================
    // PASS 2: Exact Morton Range Search (Jumps gaps via BIGMIN)
    // =========================================================================
    float maxRadius = sqrt(minDistSq[k - 1]);
    
    // Fallback for isolated VPLs where Pass 1 found fewer than k neighbors
    float3 extent = max(AABBMax - AABBMin, 0.0001f);
    if (minDistSq[k - 1] >= INFINITE) {
        maxRadius = length(extent);
    }

    float3 minNorm = saturate((vpl.position - maxRadius - AABBMin) / extent);
    float3 maxNorm = saturate((vpl.position + maxRadius - AABBMin) / extent);

    uint3 minBox = clamp(uint3(minNorm * 1024.0f), 0, 1023);
    uint3 maxBox = clamp(uint3(maxNorm * 1024.0f), 0, 1023);

    uint minCode = calculateMortonCode3D_uint3(minBox);
    uint maxCode = calculateMortonCode3D_uint3(maxBox);

    // Initial binary search to jump straight to minCode
    uint currIdx = LowerBound(minCode, 0, vplCacheSize);
    uint endIdx  = vplCacheSize; 

    while (currIdx < endIdx) {
        uint code = sortedMortonCodes[currIdx];
        
        // Exceeded maximum possible Morton code for the AABB
        if (code > maxCode) break;

        uint3 p = decodeMorton3D(code);

        // Test spatial containment inside the 3D Grid AABB
        if (p.x >= minBox.x && p.x <= maxBox.x &&
            p.y >= minBox.y && p.y <= maxBox.y &&
            p.z >= minBox.z && p.z <= maxBox.z) 
        {
            // Skip points already checked in Pass 1
            if (abs((int)currIdx - (int)launchID) > SEARCH_WINDOW) {
                uint testIndex = sortedIndices[currIdx];
                float3 diff = vplCache[testIndex].position - vpl.position;
                InsertDistance(minDistSq, k, dot(diff, diff));
            }
            currIdx++;
        } else {
            // Out of bounds: Compute next valid Morton code inside box and jump
            uint nextCode = GetNextValidMorton(code, minBox, maxBox);
            currIdx = LowerBound(nextCode, currIdx + 1, endIdx);
        }
    }

    // --- PDF Calculation ---
    float rk_sq = max(minDistSq[k - 1], 1e-8f);
    float pdf = (float)k / ((float)vplCacheSize * M_PI * rk_sq);

    vplCache[vplIndex].weightedRadiance = vplRadiance.radiance / pdf;
}