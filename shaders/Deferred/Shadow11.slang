import Common;

// struct VS_OUTPUT
// {
//     float4 position : SV_Position;
//     uint viewportIndex : SV_ViewportArrayIndex;
//     uint renderTargetIndex : SV_RenderTargetArrayIndex;
// };


// binding B,S means that the resource is bound to binding B, set S

// set 0 ... per frame

[[vk::binding(0, 0)]]
ConstantBuffer<UniformBufferFrame> gParamsFrame;    // TODO: Is this needed?

[[vk::binding(1, 0)]]
StructuredBuffer<ShadowIndex> gShadowIndex;

//----------------------------------------------------------------------------

// set 1 ... per object

[[vk::binding(0, 1)]]
ConstantBuffer<UniformBufferObjectTexture> gParamsObject;

//----------------------------------------------------------------------------

[[vk::push_constant]]
ShadowImageOffset offset;

//----------------------------------------------------------------------------

[shader("vertex")]
float4 main(float3 positionL: POSITION, int instanceID: SV_InstanceID) {
    float4 positionW = mul(gParamsObject.model, float4(positionL, 1.0));
    float4 output = mul(gShadowIndex[offset.shadowIndexOffset + instanceID].lightSpaceMatrix, positionW);
    return output;
}

// [shader("vertex")]
// VS_OUTPUT main(float3 positionL: POSITION, int instanceID: SV_InstanceID )
// {
//     VS_OUTPUT output;
//     float4 positionW = mul(gParamsObject.model, float4(positionL, 1.0));
//     output.position = mul(gShadowIndex[offset.shadowIndexOffset + instanceID].lightSpaceMatrix, positionW);
//     output.viewportIndex = gShadowIndex[offset.shadowIndexOffset + instanceID].viewportIndex;
//     output.renderTargetIndex = instanceID;
//     return output;
// }

// Shader only runs vertex stage! No frag shader needed!
