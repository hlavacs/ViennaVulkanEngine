import Common;

struct CoarseVertex {
    float3 positionW : POSITION;
};

// binding B,S means that the resource is bound to binding B, set S

// set 0 ... per frame

[[vk::binding(0, 0)]]
ConstantBuffer<UniformBufferFrame> gParamsFrame;

[[vk::binding(1, 0)]]
StructuredBuffer<Light> gLights;

[[vk::binding(2, 0)]]
RWTexture2DArray<uint,1> shadowMapArray[MAX_ARRAY_SIZE]; // shadow maps for the lights

//----------------------------------------------------------------------------

// set 1 ... per object

[[vk::binding(0, 1)]]
ConstantBuffer<UniformBufferObjectTexture> gParamsObject;

//----------------------------------------------------------------------------

[[vk::push_constant]]
ShadowIndex shadowParam;

//----------------------------------------------------------------------------

[shader("fragment")]
float4 fragmentMain(float3 positionW: POSITION, float3 svPosition: SV_Position, uint instanceID: SV_InstanceID )
{

    
    uint2 uv = (uint2)((svPosition.xy / 2.0 + 0.5) * (float2)shadowParam.mapResolution) + shadowParam.layerOffset;
    shadowMapArray[shadowParam.arrayIndex][uint3(uv, shadowParam.layerIndex)] = toDepthMap(svPosition.z);
    return float4(0,0,0,0);
}







