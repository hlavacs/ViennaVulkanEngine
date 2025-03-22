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

void storeLight(Light light, int lightIndex, float3 positionW) {
    ShadowIndex shadowIndex = light.shadowIndex[lightIndex];
    float4 positionS = mul(light.lightSpaceMatrix[lightIndex], float4(positionW, 1.0));
    positionS.xyz /= positionS.w;
    if (abs(positionS.x) > 1.0 || abs(positionS.y) > 1.0 || abs(positionS.z - 0.5) > 0.5) return;
    uint2 uv = (uint2)((positionS.xy / 2.0 + 0.5) * (float2)shadowIndex.mapResolution) + shadowIndex.layerOffset;
    shadowMapArray[shadowIndex.arrayIndex][uint3(uv, shadowIndex.layerIndex)] = floatToUNORM(positionS.z);
}

void storePointLight(Light light, float3 positionW) {
    for (int i = 0; i < 6; ++i) {
        storeLight(light, i, positionW);
    }
}

void storeDirectionalLight(Light light, float3 positionW) {
    for (int i = 0; i < 3; ++i) {
        storeLight(light, i, positionW);
    }
}

void storeSpotLight(Light light, float3 positionW) {
    storeLight(light, 0, positionW);
}

[shader("fragment")]
float4 fragmentMain(float3 positionW: POSITION, float3 positionNDC: SV_Position, uint instanceID: SV_InstanceID )
{
    Light light = gLights[instanceID];
    if (light.param.param.x == 1) storePointLight(light, positionW);
    if (light.param.param.x == 2) storeDirectionalLight(light, positionW);
    if (light.param.param.x == 3) storeSpotLight(light, positionW);
    discard;
    return float4(0,0,0,0);
}







