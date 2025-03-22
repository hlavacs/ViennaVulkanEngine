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

void storePointLight(Light light, float3 positionW) {
    // uint2 uv = (uint2)((svPosition.xy / 2.0 + 0.5) * (float2)shadowParam.mapResolution) + shadowParam.layerOffset;
    // shadowMapArray[shadowParam.arrayIndex][uint3(uv, shadowParam.layerIndex)] = toDepthMap(svPosition.z);
}

void storeDirectionalLight(Light light, float3 positionW) {
    // uint2 uv = (uint2)((svPosition.xy / 2.0 + 0.5) * (float2)shadowParam.mapResolution) + shadowParam.layerOffset;
    // shadowMapArray[shadowParam.arrayIndex][uint3(uv, shadowParam.layerIndex)] = toDepthMap(svPosition.z);
}

void storeSpotLight(Light light, float3 positionW) {
    ShadowIndex shadowIndex = light.shadowIndex[0];
    float4 positionL = mul(light.lightSpaceMatrix[0], float4(positionW, 1.0));
    uint2 uv = (uint2)((positionL.xy / 2.0 + 0.5) * (float2)shadowIndex.mapResolution) + shadowIndex.layerOffset;
    shadowMapArray[shadowIndex.arrayIndex][uint3(uv, shadowIndex.layerIndex)] = toDepthMap(positionL.z);
}


[shader("fragment")]
float4 fragmentMain(float3 positionW: POSITION, float3 positionNDC: SV_Position, uint instanceID: SV_InstanceID )
{
    Light light = gLights[instanceID];
    if (light.param.param.x == 1) storePointLight(light, positionW);
    if (light.param.param.x == 2) storeDirectionalLight(light, positionW);
    if (light.param.param.x == 3) storeSpotLight(light, positionW);
    return float4(0,0,0,0);
}







