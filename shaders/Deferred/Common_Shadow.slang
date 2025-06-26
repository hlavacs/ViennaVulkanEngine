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
