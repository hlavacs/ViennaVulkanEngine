import Common;

struct CoarseVertex {
    float3 positionW : POSITION;
};

// binding B,S means that the resource is bound to binding B, set S

// set 0 ... per frame

[[vk::binding(0, 0)]]
ConstantBuffer<UniformBufferFrame> gParamsFrame;

[[vk::binding(1, 0)]]
ConstantBuffer<Light[MAX_LIGHTS]> gLights;

[[vk::binding(2, 0)]]
RWTexture2DArray shadowMapArray[MAX_LIGHTS]; // shadow maps for the lights

//----------------------------------------------------------------------------

// set 1 ... per object

[[vk::binding(0, 1)]]
ConstantBuffer<UniformBufferObjectTexture> gParamsObject;

//----------------------------------------------------------------------------

[[vk::push_constant]]
ShadowPushConstant shadowPushConstants;

//----------------------------------------------------------------------------

[shader("fragment")]
float4 fragmentMain(float3 positionW: POSITION, uint instanceID: SV_InstanceID )
{
    return float4(0,0,0,0);
}







