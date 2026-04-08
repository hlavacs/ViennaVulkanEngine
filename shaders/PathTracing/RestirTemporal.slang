import Defines;
import Materials;
import Lightsource;
import Random;
import Frame;
import BSDF;
import ColorSpace;
import Surface;
import raytracing;
import Reservoir;

[[vk::binding(0, 2)]]
RWTexture2D<float4> inAlbedoAlpha;

[[vk::binding(1, 2)]]
RWTexture2D<float4> inNormal;

[[vk::binding(2, 2)]]
RWTexture2D<float4> inSpec;

[[vk::binding(3, 2)]]
RWTexture2D<float4> inPosition;

[[vk::binding(4, 2)]]
RWTexture2D<float4> inShadingNormal;

[[vk::binding(5, 2)]]
RWTexture2D<float4> inReprojectionError;

[[vk::binding(6, 2)]]
RWTexture2D<uint> inLightIndex;

[[vk::binding(7, 2)]]
RWTexture2D<float> inLightWeight;

[[vk::binding(8, 2)]]
RWTexture2D<float> inWeightSum;

[[vk::binding(9, 2)]]
RWTexture2D<uint> inLightCount;

[[vk::binding(10, 2)]]
RWTexture2D<uint> outLightIndex;

[[vk::binding(11, 2)]]
RWTexture2D<float> outLightWeight;

[[vk::binding(12, 2)]]
RWTexture2D<float> outWeightSum;

[[vk::binding(13, 2)]]
RWTexture2D<uint> outLightCount;

void getInitialSurfaceData(in int2 launchID, in int2 launchSize, inout IntersectionRecord its, inout MaterialRecord material) {
    RayDesc cameraRay = getInitialRay(launchID, launchSize);

    its.position = inPosition[int2(launchID)].xyz;
    its.normal = inNormal[int2(launchID)].xyz;
    its.frame = Frame(its.normal);
    its.direction = cameraRay.Direction;

    
    material.albedo = inAlbedoAlpha[launchID].xyz;
    material.alpha = inAlbedoAlpha[launchID].w;
    // Incorrect but not needed
    material.normal = float3(0.0, 0.0, 1.0);
    material.roughness = inSpec[launchID].x;
    material.metalness = inSpec[launchID].y;
    material.ior = inSpec[launchID].z;

    float3 shadingNormal = inShadingNormal[int2(launchID)].xyz;

    if (dot(its.normal, its.direction) > 0.0) {
        its.normal *= -1.0;
        shadingNormal *= -1.0;
    }

    material.shadingFrame = Frame(shadingNormal);
}

void storeReservoir(in Reservoir reservoir, uint2 launchID) {
    outLightIndex[launchID] = reservoir.lightSelected;
    outLightWeight[launchID] = reservoir.lightWeight;
    outWeightSum[launchID] = reservoir.W_sum;
    outLightCount[launchID] = reservoir.M;
}

Reservoir readReservoir(uint2 launchID) {
    Reservoir reservoir;
    reservoir.lightSelected = inLightIndex[launchID];
    reservoir.lightWeight = inLightWeight[launchID];
    reservoir.W_sum = inWeightSum[launchID];
    reservoir.M = inLightCount[launchID];

    return reservoir;
}




// Ray generation shader - entry point for each pixel
[shader("raygeneration")]
void rgenMain()
{
    // Output: writes the final color to outImage.

    // Get the current pixel coordinates and image dimensions
    int2 launchID = (int2)DispatchRaysIndex().xy;        // Current pixel (x,y)
    int2 launchSize = (int2)DispatchRaysDimensions().xy; // Image size (width,height)
    
    float3 position = inPosition[int2(launchID)].xyz;

    const uint CANDIDATE_COUNT = 32;
    const uint MAX_TEMPORAL_INFLUENCE = 20;

    if (isSkyPixel(position)) {
        Reservoir reservoir = Reservoir(0, 0.0, 0.0, 0);
        storeReservoir(reservoir, launchID);
    } else {
        uint linear = (uint)launchID.x + (uint)launchID.y * (uint)launchSize.x;
        uint pixelSeed = Hash(linear ^ (seed * 0x9E3779B9u));

        Sampler sampler = Sampler(pixelSeed);

        float lightPdf = 1.0 / float(lightCount);
        float p_hat = 0;

        MaterialRecord material;
        IntersectionRecord its;

        getInitialSurfaceData(launchID, launchSize, its, material);

        Reservoir reservoir = Reservoir(0, 0.0, 0.0, 0);

        // initial selection of 1 light of M
        for (uint i = 0; i < CANDIDATE_COUNT; i++)
        {
            uint lightIndex = sampler.UniformUIntFast(0, lightCount - 1);

            p_hat = unoccludedLightRadiance(lightIndex, its, material, sampler);

            float w = p_hat / lightPdf;

            UpdateReservoir(reservoir, lightIndex, w, 1, sampler);
        }

        if (IsReservoirValid(reservoir))
        {
            // Update weight with new radiance with visibility term!
            float p_hat = length(groundTruthLightRadiance(reservoir.lightSelected, its, material, sampler));
            reservoir.lightWeight = p_hat > 0.0 ? (1.0 / p_hat) * reservoir.W_sum / reservoir.M : 0.0;
        } else {
            reservoir = Reservoir(0, 0.0, 0.0, 0);
        }

        //reject history if reprojection error is too big
        //better system needed in the future
        if (inReprojectionError[launchID].x < 0.5) {
            Reservoir temporalReservoir = readReservoir(launchID);

            // restrict influence from past samples.
            // this may not be optimal, the real time raytracing engine handles this diffrently
            temporalReservoir.M = min(MAX_TEMPORAL_INFLUENCE * reservoir.M, temporalReservoir.M);

            reservoir = CombineReservoirs(reservoir, temporalReservoir, its, material, sampler);
        }

        storeReservoir(reservoir, launchID);  
    }
}
