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
}

//-----------------------------------------------------------------------

// // Shadow map generation shaders
// export public float calculateShadow(Sampler2DArray<uint, 1> shadowMap, ShadowIndex shadowIdx, float4 lightSpacePos) {
//     // Perspective divide for projection space
//     float3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
//     projCoords = projCoords / 2.0 + 0.5; // Transform to [0, 1] range
//     uint2 uv = (uint2)(projCoords.xy * (float2)shadowIdx.mapResolution) + shadowIdx.layerOffset;

//     // Retrieve shadow map depth value
//     float shadowDepth = shadowMap.Sample(uint3(uv, shadowIdx.layerIndex));

//     // Compare and return shadow factor
//     return (lightSpacePos.z > shadowDepth + 0.005) ? 0.3 : 1.0; // 0.3 for shadowed, 1.0 for lit
// }

// export public float3 applyShadow(Camera camera, Light light, ShadowIndex shadowIndex, uint lightSpaceIndex, float3 positionW, Sampler2DArray<uint, 1> shadowMap, ShadowIndex shadowIdx) {
//     float4 lightSpacePos = mul(shadowIdx.lightSpaceMatrix, float4(positionW, 1.0)); // Replace with actual transformation to light space
//     float shadowFactor = calculateShadow(shadowMap, shadowIdx, lightSpacePos);
//     return shadowFactor;
// }

// float calcPointShadow(float3 positionW, float3 lightPosW, TextureCubeArray<float> shadowCubes, SamplerComparisonState shadowSampler, uint arrayIndex) {
//     float3 fragToLight = normalize(lightPosW - positionW);
//     float currentDepth = length(lightPosW - positionW) / 15;
//     float far = 15;
//     float bias = 0.05;

//     float shadow = shadowCubes.SampleCmpLevelZero(
//         shadowSampler,
//         float4(-fragToLight, arrayIndex),
//         currentDepth - bias
//     );

//     return shadow;
// }

float calcPointShadow(float3 positionW, float3 lightPosW, TextureCubeArray<float> shadowCubes, SamplerState shadowSampler, uint arrayIndex) {
    float3 fragToLight = positionW - lightPosW;
    
    float far = 25;
    float bias = 0.05;

    float closestDepth = shadowCubes.Sample(shadowSampler, float4(fragToLight, arrayIndex)).x;

    //closestDepth *= far;

    float currentDepth = length(fragToLight);

    float shadow = currentDepth - bias > closestDepth ? 1.0 : 0.0;

    // TODO: Should return shadow 1 or 0, but this is the best result yet to continue experimenting
    return closestDepth;
}

float LinearizeDepth(float depth)
{
    float n = 1;
    float f = 25;
    float z = depth;
    return (2.0 * n) / (f + n - z * (f - n));
}
