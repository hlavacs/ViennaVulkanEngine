import Common;

struct CoarseVertex {
    float3 positionW : POSITION;
    float3 normalW : NORMAL;
    float2 uv : TEXCOORD;
    float3 tangentW : TANGENT;
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

// set 1 ... per object

// binding 0,1 means that the uniform buffer is bound to binding 0, set 1
[[vk::binding(0, 1)]]
ParameterBlock<UniformBufferObjectTexture> gParamsObject;

// binding 1,1 means that the Texture/Sampler is bound to binding 1, set 1
[[vk::binding(1,1)]]
Sampler2D texSampler;

[shader("fragment")]
float4 fragmentMain(CoarseVertex cv : CoarseVertex) : SV_Target 
{
    Camera camera = gParamsFrame.camera;
    uint3 numberLights = gParamsFrame.numberLights;

    float4 tex = texSampler.Sample(cv.uv);
    float3 l = calculateLighting(gLights, numberLights, cv.normalW, camera.positionW, cv.positionW);
    return float4( tex.xyz*l, tex.w );
}







