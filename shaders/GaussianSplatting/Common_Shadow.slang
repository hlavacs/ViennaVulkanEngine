// Common_Shadow.slang - Shadow calculation functions
// Copy of Deferred/Common_Shadow.slang for gaussian splatting IBL shaders
implementing Common;

export public struct LightSpaceMatrix {
    public float4x4 lightSpaceMatrix;
    public float3 lightPosition;
    private float pad;
}

//-----------------------------------------------------------------------

float calcPointShadow(float3 positionW, float3 lightPosW, SamplerCubeArrayShadow cubeArrayShadow, uint arrayIndex) {
    float3 L = positionW - lightPosW;
    float dist = length(L);

    float3 dir = normalize(L);
    float4 coord = float4(dir, arrayIndex);

    constexpr float farPlane = 1000;
    float refDepth = dist / farPlane;

    float shadow = cubeArrayShadow.SampleCmpLevelZero(coord, refDepth);

    return shadow; // TODO: 0 or other value, like 0.3?
}

float calcDirectShadow(float3 positionW, float3 lightPosW, Sampler2DArrayShadow shadow2DArray, uint arrayIndex, StructuredBuffer<float4x4> lsm) {
    float4x4 lightSpaceMatrix = lsm[arrayIndex];
    float4 lightSpace = mul(lightSpaceMatrix, float4(positionW, 1.0));
    float3 ndc = lightSpace.xyz / lightSpace.w;
    float2 uv = ndc.xy * 0.5 + 0.5;

    float3 L = positionW - lightPosW;
    float dist = length(L);
    float bias = 0.0001;

    constexpr float farPlane = 1000;
    float refDepth = dist / farPlane;
    float shadow = shadow2DArray.SampleCmpLevelZero(float3(uv, arrayIndex), refDepth + bias);

    return shadow; // TODO: 0 or other value, like 0.3?
}

float calcSpotShadow(float3 positionW, float3 lightPosW, Sampler2DArrayShadow shadow2DArray, uint arrayIndex, StructuredBuffer<float4x4> lsm) {
    float4x4 lightSpaceMatrix = lsm[arrayIndex];
    float4 lightSpace = mul(lightSpaceMatrix, float4(positionW, 1.0));
    float3 ndc = lightSpace.xyz / lightSpace.w;
    float2 uv = ndc.xy * 0.5 + 0.5;

    float3 L = positionW - lightPosW;
    float dist = length(L);

    constexpr float farPlane = 1000;
    float refDepth = dist / farPlane;
    float shadow = shadow2DArray.SampleCmpLevelZero(float3(uv, arrayIndex), refDepth);

    return shadow; // TODO: 0 or other value, like 0.3?
}
