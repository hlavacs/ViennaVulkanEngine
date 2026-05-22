import LightVertex;

[[vk::binding(0, 0)]]
RWStructuredBuffer<LightVertex> lightVertexCache;

[[vk::binding(1, 0)]]
RWStructuredBuffer <Atomic<float>> importanceSum;

//The compute shader cpu side has the tread count per work group hardcoded this needs to be changed!!!!!!!!
[shader("compute")]
[numthreads(256, 1, 1)]
void main(uint3 threadId: SV_DispatchThreadID) {
    int2 launchID = threadId.xy;  

    LightVertex lightVertex = lightVertexCache[launchID.x];

    // needs VK_EXT_shader_atomic_float and VK_EXT_subgroup_optimize_atomic_reduce
    importanceSum[0].add(lightVertex.importance);
}

