import Common;

struct CoarseVertex {
    float3 positionW : POSITION;
    float3 normalW : NORMAL;
    float4 fragColor : COLOR;
};

// set 0 ... per frame

// binding 0,1 means that the uniform buffer is bound to binding 0, set 0
[[vk::binding(0, 0)]]
ParameterBlock<UniformBufferFrame> gParamsFrame;

// binding 1,0 means that the uniform buffer is bound to binding 1, set 0
[[vk::binding(1, 0)]]
ConstantBuffer<Light[MAX_LIGHTS]> gLights;

// binding 2,0 means that the uniform buffer is bound to binding 2, set 0
[[vk::binding(2, 0)]]
Texture2DArray shadowMapArray[MAX_SHADOW_MAPS]; // shadow maps for the lights

// binding 1,0 means that the uniform buffer is bound to binding 1, set 0
[[vk::binding(3, 0)]]
SamplerState shadowSampler; // shadow map sampler

//----------------------------------------------------------------------------

[shader("fragment")]
float4 fragmentMain(CoarseVertex cv : CoarseVertex) : SV_Target
{
    Camera camera = gParamsFrame.camera;
    uint3 numberLights = gParamsFrame.numberLights;
    float3 l = calculateLighting(gLights, numberLights, cv.normalW, camera.positionW, cv.positionW);
    return float4(l, 1) * cv.fragColor;
}


