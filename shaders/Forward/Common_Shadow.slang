implementing Common;

//-----------------------------------------------------------------------

export public struct ShadowIndex {
    public int2 mapResolution;
    public int layerIndex;
    public int viewportIndex;
    public int2 layerOffset;
    public float4x4 lightSpaceMatrix;
};

export public uint floatToUNORM(float z) {
    return (uint)(z * (float)(0xFFFFFFFF));
}

export public float UNORMTofloat(uint Z) {
    return (float)Z / (float)(0xFFFFFFFF);
}

//-----------------------------------------------------------------------

// Shadow map generation shaders
export public float calculateShadow(RWTexture2DArray<uint, 1> shadowMap, ShadowIndex shadowParam, float4 lightSpacePos) {
    // Perspective divide for projection space
    float3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    projCoords = projCoords / 2.0 + 0.5; // Transform to [0, 1] range
    uint2 uv = (uint2)(projCoords.xy * (float2)shadowParam.mapResolution) + shadowParam.layerOffset;

    // Retrieve shadow map depth value
    float shadowDepth = UNORMTofloat(shadowMap.Load(uint3(uv, shadowParam.layerIndex)).r);

    // Compare and return shadow factor
    return (lightSpacePos.z > shadowDepth + 0.005) ? 0.3 : 1.0; // 0.3 for shadowed, 1.0 for lit
}

export public float3 applyShadow(Camera camera, Light light, ShadowIndex shadowIndex, uint lightSpaceIndex, float3 positionW, RWTexture2DArray<uint, 1> shadowMap, ShadowIndex shadowParam) {
    float4 lightSpacePos = mul(shadowIndex.lightSpaceMatrix, float4(positionW, 1.0)); // Replace with actual transformation to light space
    float shadowFactor = calculateShadow(shadowMap, shadowParam, lightSpacePos);
    return shadowFactor;
}


