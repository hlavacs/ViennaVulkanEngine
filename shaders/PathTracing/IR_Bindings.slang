import IR_Defines;

[[vk::binding(5, 1)]]
RWStructuredBuffer<VPLReconnectionInfo> vplCache;

[[vk::binding(6, 1)]]
cbuffer UniformBufferBidirectional
{
    uint vplCacheSize;
    float3 AABBMin;
    float3 AABBMax;
};
