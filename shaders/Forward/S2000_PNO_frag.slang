import Common;

struct CoarseVertex {
    float3 normal;
    float4 fragColor;
};

// set 0 ... per frame

// binding 0,1 means that the uniform buffer is bound to binding 0, set 0
[[vk::binding(0, 0)]]
ParameterBlock<UniformBufferFrame> gParamsFrame;

// binding 1,0 means that the uniform buffer is bound to binding 1, set 0
[[vk::binding(1, 0)]]
ConstantBuffer<Light[MAX_LIGHTS]> gLights;

// binding 1,0 means that the uniform buffer is bound to binding 1, set 0
[[vk::binding(2, 0)]]
SamplerState shadowSampler; // shadow map sampler

// binding 2,0 means that the uniform buffer is bound to binding 2, set 0
[[vk::binding(3, 0)]]
Texture2DArray myTextureArrays[MAX_LIGHTS]; // one shadow map array per light


[shader("fragment")]
float4 fragmentMain(CoarseVertex coarseVertex : CoarseVertex) : SV_Target 
{
    return coarseVertex.fragColor;
}


