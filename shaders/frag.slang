struct CoarseVertex
{
    float3 fragColor;
    float2 uv;
};

struct UniformBufferObject {
    float4x4 model;
    float4x4 view;
    float4x4 proj;
};

// binding 0,0 means that the uniform buffer is bound to binding 0, set 0
[[vk::binding(0, 0)]]
ParameterBlock<UniformBufferObject> gParams;

// binding 0,0 means that the uniform buffer is bound to binding 1, set 0
[[vk::binding(1,0)]]
Sampler2D texSampler;


[shader("fragment")]
float4 fragmentMain(CoarseVertex coarseVertex : CoarseVertex) : SV_Target 
{
    return texSampler.Sample(coarseVertex.uv);
}







