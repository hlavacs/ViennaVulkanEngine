import LightVertex;

[[vk::binding(0, 0)]]
RWStructuredBuffer<LightVertex> lightVertexCache;

[[vk::binding(1, 0)]]
RWStructuredBuffer <Atomic<float>> importanceSum;

[[vk::binding(2, 0)]]
RWStructuredBuffer<Atomic<float>> keepSum;

/*
[[vk::binding(3, 0)]]
RWStructuredBuffer<Atomic<float>> importanceMax;

[[vk::binding(4, 0)]]
RWStructuredBuffer<Atomic<float>> badnessSum;
*/

[shader("compute")]
[numthreads(1, 1, 1)]
void main(uint3 threadId: SV_DispatchThreadID) {
    importanceSum[0] = 0.0;
    keepSum[0] = 0.0;
    //importanceMax[0] = 0.0;
    //badnessSum[0] = 0.0;
}