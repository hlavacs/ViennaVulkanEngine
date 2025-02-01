import Common;

struct CoarseVertex {
    float3 normal;
    float2 uv;
    float3 tangent;
};

// set 0 ... per frame

// binding 1,0 means that the uniform buffer is bound to binding 1, set 0
[[vk::binding(1, 0)]]
public SamplerState shadowSampler; // shadow map sampler

// binding 2,0 means that the uniform buffer is bound to binding 2, set 0
[[vk::binding(2, 0)]]
public Texture2DArray myTextureArrays[MAX_LIGHTS]; // one shadow map array per light

// set 1 ... per object

// binding 0,1 means that the uniform buffer is bound to binding 0, set 1
[[vk::binding(0, 1)]]
ParameterBlock<UniformBufferObjectTexture> gParamsObject;

// binding 1,1 means that the Texture/Sampler is bound to binding 1, set 1
[[vk::binding(1,1)]]
Sampler2D texSampler;

[shader("fragment")]
float4 fragmentMain(CoarseVertex coarseVertex : CoarseVertex) : SV_Target 
{
    return texSampler.Sample(coarseVertex.uv);
}







