import Common;

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
