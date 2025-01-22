struct CoarseVertex {
    float3 fragColor;
    float2 uv;
    float3 normal;
    float3 tangent;
};

struct UniformBufferObject {
    float4x4 model;
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
    return float4( coarseVertex.fragColor, 1.0 );
}


