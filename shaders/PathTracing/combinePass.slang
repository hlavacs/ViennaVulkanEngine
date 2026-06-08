import ColorSpace;

[[vk::binding(0, 0)]]
RWTexture2D<float4> inLighting;

[[vk::binding(1, 0)]]
RWTexture2D<float4> inAlbedoAlpha;

[[vk::binding(2, 0)]]
RWTexture2D<float4> inReprojectedLighting;

[[vk::binding(3, 0)]]
RWTexture2D<float4> outPrevLighting;

[[vk::binding(4, 0)]]
RWTexture2D<float4> inReprojectionError;

[[vk::binding(5, 0)]]
RWTexture2D<float4> outCombined;

[[vk::binding(6, 0)]]
RWTexture2D<float4> outAccumulatedLight;

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 threadId: SV_DispatchThreadID) {
    int2 launchID = threadId.xy;

    float3 albedo = inAlbedoAlpha[launchID].xyz;
    float3 lighting = inLighting[launchID].xyz;
    float3 lightingPrev = inReprojectedLighting[launchID].xyz;

    float reprojectionError = inReprojectionError[launchID].x;
    float maxReprojectionError = 0.5;

    float reprojectionErrorFactor = clamp(reprojectionError / maxReprojectionError, 0.0, 1.0);

    float accumulationBaseFactor = 0.05;
    float accumulationFactor = lerp(accumulationBaseFactor, 1.0, reprojectionErrorFactor);

    lighting = clamp(lighting, 0.0, 1000.0);

    float3 lightingAccumulated = lightingPrev * (1.0 - accumulationFactor) + lighting * accumulationFactor;

    lightingAccumulated = max(min(lightingAccumulated, 100.0), 0.0);

    // put lightAccumulated here!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    outAccumulatedLight[launchID] = float4(lightingAccumulated, 1.0);
    outCombined[launchID] = float4(LinearToSRGB(lightingAccumulated * max(albedo, 0.001)), 1.0);


    //outCombined[launchID] = float4(albedo.z, albedo.z, albedo.z, 1.0);

    //outCombined[launchID] = float4(LinearToSRGB(lighting), 1.0);
}