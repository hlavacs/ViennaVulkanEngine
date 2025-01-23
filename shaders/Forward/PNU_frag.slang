struct CoarseVertex {
    float3 normal;
    float2 uv;
};

struct UniformBufferObject {
    float4x4 model;
    float4x4 modelInvTranspose;
};

// binding 0,1 means that the uniform buffer is bound to binding 0, set 1
[[vk::binding(0, 1)]]
ParameterBlock<UniformBufferObject> gParamsObject;

// binding 1,1 means that the uniform buffer is bound to binding 1, set 1
[[vk::binding(1,1)]]
Sampler2D texSampler;

[shader("fragment")]
float4 fragmentMain(CoarseVertex coarseVertex : CoarseVertex) : SV_Target 
{
    return texSampler.Sample(coarseVertex.uv);
}







