implementing Common;

// Shadow map generation shaders
export public float calculateShadow(Sampler2D shadowMap, float3 lightSpacePos) {
    // Perspective divide for projection space
    float3 projCoords = lightSpacePos / lightSpacePos.z;
    projCoords = projCoords * 0.5 + 0.5; // Transform to [0, 1] range

    // Retrieve shadow map depth value
    float shadowDepth = shadowMap.Sample(projCoords.xy).r;

    // Compare and return shadow factor
    return (projCoords.z > shadowDepth + 0.005) ? 0.3 : 1.0; // 0.3 for shadowed, 1.0 for lit
}

export public float3 applyShadow(Light light[], float3 lighting, float3 fragPos, float3 normal, Sampler2D shadowMap) {
    float3 lightSpacePos = float3(0.0); // Replace with actual transformation to light space
    float shadowFactor = calculateShadow(shadowMap, lightSpacePos);
    return shadowFactor * lighting;
}


