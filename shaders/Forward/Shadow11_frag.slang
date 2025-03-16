import Common;

struct CoarseVertex {
    float3 positionW : POSITION;
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
RWTexture2DArray shadowMapArray[MAX_SHADOW_MAPS]; // shadow maps for the lights

//----------------------------------------------------------------------------

// set 1 ... per object

// binding 0,1 means that the uniform buffer is bound to binding 0, set 1
[[vk::binding(0, 1)]]
ParameterBlock<UniformBufferObjectTexture> gParamsObject;

//----------------------------------------------------------------------------

[shader("fragment")]
float4 fragmentMain(float3 positionW: POSITION, uint instanceID: SV_InstanceID )
{
    return float4(0,0,0,0);
}







