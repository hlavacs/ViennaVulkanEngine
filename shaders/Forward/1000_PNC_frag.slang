import Common;

struct CoarseVertex {
    float3 positionW : POSITION;
    float3 normalW : NORMAL;
    float4 fragColor : COLOR;
};

// binding B,S means that the resource is bound to binding B, set S

// set 0 ... per frame

[[vk::binding(0, 0)]]
ConstantBuffer<UniformBufferFrame> gParamsFrame;

[[vk::binding(1, 0)]]
ConstantBuffer<Light[MAX_LIGHTS]> gLights;

[[vk::binding(2, 0)]]
Texture2DArray shadowMapArray; // shadow maps for the lights

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


