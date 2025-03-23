import Common;

struct VS_OUTPUT
{
    float4 position : SV_Position;
    uint viewportIndex : SV_ViewportArrayIndex;
    uint renderTargetIndex : SV_RenderTargetArrayIndex;
};

// binding B,S means that the resource is bound to binding B, set S

// set 0 ... per frame

[[vk::binding(0,0)]]
ConstantBuffer<UniformBufferFrame> gParamsFrame;

[[vk::binding(1, 0)]]
StructuredBuffer<ShadowIndex> gShadowIndex;

//----------------------------------------------------------------------------

// set 1 ... per object

[[vk::binding(0,1)]]
ConstantBuffer<UniformBufferObjectTexture> gParamsObject;

//----------------------------------------------------------------------------

[[vk::push_constant]]
ShadowImageArrayOffset offset;

//----------------------------------------------------------------------------

[shader("vertex")]
VS_OUTPUT vertexMain(float3 positionL: POSITION, int instanceID: SV_InstanceID )
{
    VS_OUTPUT output;
    float4 positionW = mul(gParamsObject.model, float4(positionL, 1.0));
    output.position = mul(gShadowIndex[offset.shadowIndexOffset + instanceID].lightSpaceMatrix, positionW);
    output.viewportIndex = gShadowIndex[offset.shadowIndexOffset + instanceID].viewportIndex;
    output.renderTargetIndex = instanceID;

    return output;
}



