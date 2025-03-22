implementing Common;

//-----------------------------------------------------------------------

export public struct ShadowPushConstant {
    public uint2 mapResolution;
    public uint arrayIndex;
    public uint layerIndex;
    public uint2 layerOffset;
};

//-----------------------------------------------------------------------

// Shadow map generation shaders
export public float calculateShadow(RWTexture2DArray<uint, 1> shadowMap, ShadowPushConstant shadowParam, float4 lightSpacePos) {
    // Perspective divide for projection space
    float3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    projCoords = projCoords / 2.0 + 0.5; // Transform to [0, 1] range
    uint2 uv = (uint2)(projCoords.xy * (float2)shadowParam.mapResolution);

    // Retrieve shadow map depth value
    float shadowDepth = shadowMap.Load(uint3(uv + shadowParam.layerOffset, shadowParam.layerIndex)).r;

    // Compare and return shadow factor
    return (projCoords.z > shadowDepth + 0.005) ? 0.3 : 1.0; // 0.3 for shadowed, 1.0 for lit
}

export public float3 applyShadow(Light light, uint lightSpaceIndex, float3 fragPosW, RWTexture2DArray<uint, 1> shadowMap, ShadowPushConstant shadowParam) {
    float4 lightSpacePos = mul( light.lightSpaceMatrix[lightSpaceIndex], float4( fragPosW, 1.0)); // Replace with actual transformation to light space
    float shadowFactor = calculateShadow(shadowMap, shadowParam, lightSpacePos);
    return shadowFactor;
}


