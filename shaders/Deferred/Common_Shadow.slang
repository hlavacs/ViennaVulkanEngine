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


float calcPointShadow(float3 positionW, float3 lightPosW, TextureCubeArray<float> shadowCubes, SamplerComparisonState shadowSampler, uint arrayIndex) {
    float3 L = positionW - lightPosW;
    float dist = length(L);

    float3 dir = normalize(L);
    float4 coord = float4(dir, arrayIndex);

    float refDepth = dist / 1000;

    float shadow = shadowCubes.SampleCmpLevelZero(shadowSampler, coord, refDepth);

    return shadow;
}
