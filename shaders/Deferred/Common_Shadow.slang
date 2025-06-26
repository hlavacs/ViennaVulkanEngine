implementing Common;

//-----------------------------------------------------------------------

export public struct ShadowIndex {
    public int2 mapResolution;
    public uint layerIndex;
    public uint viewportIndex;
    public int2 layerOffset;
    public float4x4 lightSpaceMatrix;
};

// export public struct ShadowImageOffset {
//     public int shadowIndexOffset;
//     public int numberShadows;
// };

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

    return shadow;
}

float calcSpotShadow(float3 positionW, float3 lightPosW, SamplerCubeArrayShadow cubeArrayShadow, uint arrayIndex) {
    // float4 clip = mul(spotVP[spotIdx], float4(fragPos, 1));
    // clip.xyz /= clip.w;
    // float2 uv = clip.xy * 0.5 + 0.5;
    // float refDepth = clip.z * 0.5 + 0.5;
    // uint layer = pointLightCount + spotIdx;
    // return gShadowMaps.SampleCmpLevelZero(
    //     float4(float3(uv, 0), layer),
    //     refDepth
    // );

    return 5;
}
