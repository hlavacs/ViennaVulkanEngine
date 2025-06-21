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
ConstantBuffer<UniformBufferObject> gParamsObject;

//----------------------------------------------------------------------------

[[vk::push_constant]]
LightSpaceMatrix lsm;

//----------------------------------------------------------------------------

[shader("vertex")]
float4 main(float3 positionL: POSITION) {
    float4 positionW = mul(gParamsObject.model, float4(positionL, 1.0)); // do I need to do .xyz and set w to 1?
    float4 output = mul(lsm.lightSpaceMatrix, positionW);
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
