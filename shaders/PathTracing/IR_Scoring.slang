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

[shader("compute")]
[numthreads(256, 1, 1)]
void main(uint3 threadId: SV_DispatchThreadID) {
    int2 launchID = threadId.xy;

    // reset indices and set the score equal to how many pixels use this vpl
    score[launchID.x] = pixelUsage[launchID.x];
    indices[launchID.x] = launchID.x;
    pixelUsage[launchID.x] = 0;

    if (launchID.x == 0) {
        const float BASE_REPLACMENT_RATIO = 0.05;
        const float BASE_REPRODUCTION_RATIO = 0.02;

        limits[0].replacmentLimit = (uint)(((float)vplCacheSize) * BASE_REPLACMENT_RATIO);
        limits[0].reproductionLimit = vplCacheSize - (uint)(((float)vplCacheSize) * BASE_REPRODUCTION_RATIO);
    }

}