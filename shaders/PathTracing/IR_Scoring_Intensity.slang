import IR_Defines;

[[vk::binding(0, 0)]]
RWStructuredBuffer<uint> score;

[[vk::binding(1, 0)]]
RWStructuredBuffer<uint> indices;

[[vk::binding(2, 0)]]
cbuffer UniformBufferBidirectional
{
    uint vplCacheSize;
    float3 AABBMin;
    float3 AABBMax;
};

[[vk::binding(3, 0)]]
RWStructuredBuffer<VPLLimits> limits;

[[vk::binding(4, 0)]]
RWStructuredBuffer<uint> pixelUsage;

[[vk::binding(5, 0)]]
StructuredBuffer<VPLShading> vplRadianceCache;

[[vk::binding(6, 0)]] RWStructuredBuffer<float> nearestPDFs;

[shader("compute")]
[numthreads(256, 1, 1)]
void main(uint3 threadId: SV_DispatchThreadID) {
    int2 launchID = threadId.xy;

    // reset indices and set the score equal to how many pixels use this vpl

    float intensitsy = max(length(vplRadianceCache[launchID.x].radiance) / nearestPDFs[launchID.x], 0.0);
    uint instensityInt = asuint(intensitsy); 

    if (pixelUsage[launchID.x] <= 0) {
        instensityInt = 0;
    }

    score[launchID.x] = instensityInt;
    indices[launchID.x] = launchID.x;
    pixelUsage[launchID.x] -= 1;

    if (launchID.x == 0) {
        const float BASE_REPLACMENT_RATIO = 0.05;
        const float BASE_REPRODUCTION_RATIO = 0.02;

        limits[0].replacmentLimit = (uint)(((float)vplCacheSize) * BASE_REPLACMENT_RATIO);
        limits[0].reproductionLimit = vplCacheSize - (uint)(((float)vplCacheSize) * BASE_REPRODUCTION_RATIO);
    }

}