import IR_Defines;

[[vk::binding(0, 0)]]
RWStructuredBuffer<uint> sortedScore;

[[vk::binding(1, 0)]]
RWStructuredBuffer<VPLLimits> limits;

[[vk::binding(2, 0)]]
cbuffer UniformBufferBidirectional
{
    uint vplCacheSize;
    float3 AABBMin;
    float3 AABBMax;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void main(uint3 threadId: SV_DispatchThreadID) {
    int2 launchID = threadId.xy;

    //find the last element that is 0 and set the replacmentLimit to that index
    if (launchID.x > limits[0].replacmentLimit && launchID.x > 0) {
        if (sortedScore[launchID.x] != 0 && sortedScore[launchID.x - 1] == 0) {
            limits[0].replacmentLimit = launchID.x - 1;
        }
    }

    // find the last elemnt that is 0 and set the reproductionLimit to that index
    if (launchID.x > limits[0].reproductionLimit && launchID.x < vplCacheSize - 1) {
        if (sortedScore[launchID.x] == 0 && sortedScore[launchID.x + 1] != 0) {
            limits[0].reproductionLimit = launchID.x + 1;
        }
    }

    // check if all scores are 0 if so replace all vpls;
    if (launchID.x == vplCacheSize - 1) {
        if (sortedScore[launchID.x] == 0) {
            limits[0].replacmentLimit = vplCacheSize - 1;
            //set to index out of range to indicate to not use reproduction.
            limits[0].reproductionLimit = vplCacheSize;
        }
    }
}