import Common;

// struct VS_OUTPUT
// {
//     float4 position : SV_Position;
//     uint viewportIndex : SV_ViewportArrayIndex;
//     uint renderTargetIndex : SV_RenderTargetArrayIndex;
// };


// binding B,S means that the resource is bound to binding B, set S

// set 0 ... per frame

// [[vk::binding(0, 0)]]
// ConstantBuffer<UniformBufferFrame> gParamsFrame;    // TODO: Is this needed?

// [[vk::binding(1, 0)]]
// StructuredBuffer<ShadowIndex> gShadowIndex;

//----------------------------------------------------------------------------

// set 1 ... per object

/* 
 * NOTE: this is in practive never just a UniformBufferObject, but one of UniformBufferObject,
 * UniformBufferObjectTexture or UniformBufferObjectColor, however we only ever need
 * the model matrix - so we use the existing object ubo provided by the actual renderer
 * and just use the model matrix.
*/
[[vk::binding(0, 0)]]
ConstantBuffer<UniformBufferObject> gParamsObject;

//----------------------------------------------------------------------------

[[vk::push_constant]]
LightSpaceMatrix lsm;

//----------------------------------------------------------------------------

struct VSOut {
    float4 clipPos : SV_POSITION; 
    float3 positionW : WORLDPOS;   
}

[shader("vertex")]
VSOut main(float3 positionL: POSITION) {
    VSOut output = {};
    float4 positionW = mul(gParamsObject.model, float4(positionL, 1.0));
    output.positionW = positionW.xyz;
    output.clipPos = mul(lsm.lightSpaceMatrix, positionW);
    return output;
}

[shader("fragment")]
float main(VSOut in) : SV_Depth {
    float dist = length(in.positionW - lsm.lightPosition);

    constexpr float farPlane = 1000;
    float outDepth = clamp(dist / farPlane, 0.0, 1.0);
    return outDepth;
}
