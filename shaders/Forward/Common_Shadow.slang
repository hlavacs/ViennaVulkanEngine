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
export public float calculateShadow(RWTexture2DArray<uint, 1> shadowMap, ShadowPushConstant shadowParam, float3 lightSpacePos) {
    // Perspective divide for projection space
    float3 projCoords = lightSpacePos / lightSpacePos.z;
    projCoords = projCoords * 2.0 - 0.5; // Transform to [0, 1] range
    uint2 uv = (uint2)(projCoords.xy);

    // Retrieve shadow map depth value
    float shadowDepth = shadowMap.Load(uint3(uv, shadowParam.layerIndex)).r;

    // Compare and return shadow factor
    return (projCoords.z > shadowDepth + 0.005) ? 0.3 : 1.0; // 0.3 for shadowed, 1.0 for lit
}

export public float3 applyShadow(Light light[], float3 lighting, float3 fragPosW, RWTexture2DArray<uint, 1> shadowMap, ShadowPushConstant shadowParam) {
    float3 lightSpacePos = float3(0.0); // Replace with actual transformation to light space
    float shadowFactor = calculateShadow(shadowMap, shadowParam, lightSpacePos);
    return shadowFactor * lighting;
}


