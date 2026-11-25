import Defines;
import Materials;
import Lightsource;
import Random;
import Frame;
import BSDF;
import ColorSpace;
import Surface;
import raytracing;
import IR_Reservoir;
import IR_Restir;
import IR_Defines;
import IR_Bindings;
import IR_VPL_Evaluation;

[[vk::binding(0, 2)]]
RWTexture2D<float4> inAlbedoAlpha;

[[vk::binding(1, 2)]]
RWTexture2D<float4> inNormal;

[[vk::binding(2, 2)]]
RWTexture2D<float4> inTrueNormal;

[[vk::binding(3, 2)]]
RWTexture2D<float4> inSpec;

[[vk::binding(4, 2)]]
RWTexture2D<float4> inPosition;

[[vk::binding(5, 2)]]
RWTexture2D<float4> inShadingNormal;

[[vk::binding(6, 2)]]
RWTexture2D<float4> outImage;

[[vk::binding(7, 2)]]
RWStructuredBuffer<ReservoirVPL> inReservoirVPL;

[[vk::binding(8, 2)]]
RWStructuredBuffer<ReservoirVPL> outReservoirVPL;

[[vk::binding(9, 2)]]
RWStructuredBuffer<uint> outPixelUsage;

[[vk::binding(10, 2)]]
RWStructuredBuffer<uint2> mutationPixelIndices;

void getInitialSurfaceData(in int2 launchID, in int2 launchSize, inout IntersectionRecord its, inout MaterialRecord material) {
    RayDesc cameraRay = getInitialRay(launchID, launchSize);

    its.position = inPosition[int2(launchID)].xyz;
    its.normal = inNormal[int2(launchID)].xyz;
    its.frame = Frame(its.normal);
    its.direction = cameraRay.Direction;
    its.trueNormal = inTrueNormal[int2(launchID)].xyz;

    material.albedo = inAlbedoAlpha[launchID].xyz;
    material.alpha = inAlbedoAlpha[launchID].w;
    // Incorrect but not needed
    material.normal = float3(0.0, 0.0, 1.0);
    material.roughness = inSpec[launchID].x;
    material.metalness = inSpec[launchID].y;
    material.ior = inSpec[launchID].z;

    float3 shadingNormal = inShadingNormal[int2(launchID)].xyz;

    if (dot(its.trueNormal, its.direction) > 0.0) {
        its.trueNormal *= -1.0;
    }

    material.shadingFrame = Frame(shadingNormal);
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

    const uint MAX_SPATIAL_INFLUENCE_GI = 10;
    const uint SPATIAL_SAMPLES_GI = 12;
    const uint SAMPLE_RADIUS_GI = 12;

    if (isSkyPixel(position)) {
        ReservoirVPL reservoirLVC = createEmptyReservoirVPL();
        storeReservoirVPL(reservoirLVC, launchID, launchSize, outReservoirVPL);
    } else {
        uint linear = (uint)launchID.x + (uint)launchID.y * (uint)launchSize.x;
        uint pixelSeed = Hash(linear ^ (seed * 0x9E3779B9u));

        Sampler sampler = Sampler(pixelSeed);

        float lightPdf = 1.0 / float(lightCount);
        float p_hat = 0;

        MaterialRecord material;
        IntersectionRecord its;

        getInitialSurfaceData(launchID, launchSize, its, material);

        float3 normal = inNormal[launchID].xyz;
        float3 position = inPosition[launchID].xyz;

        float3 output = float3(0.0);

        //GI

        ReservoirVPL reservoirVPL = readReservoirVPL(launchID, launchSize, inReservoirVPL);
        VPLReconnectionInfo vpl = vplCache[reservoirVPL.vplIndex];

        for (uint i = 0; i < SPATIAL_SAMPLES_GI; i++) {
            int x = sampler.UniformUIntFast(0, 2 * SAMPLE_RADIUS_GI) - SAMPLE_RADIUS_GI;
            int y = sampler.UniformUIntFast(0, 2 * SAMPLE_RADIUS_GI) - SAMPLE_RADIUS_GI;

            int2 offset = int2(x, y);

            int2 readID = launchID + offset;
            clampReadID(readID, launchSize);

            float3 normalSpatial = inNormal[readID].xyz;
            float3 positionSpatial = inPosition[readID].xyz;

            if (dot(normal, normalSpatial) >= 0.9 && length(positionSpatial - position) < 0.5) {
                ReservoirVPL reservoirSpatialVPL = readReservoirVPL(readID, launchSize, inReservoirVPL);

                // restrict influence from past samples.
                reservoirSpatialVPL.M = min(MAX_SPATIAL_INFLUENCE_GI * reservoirVPL.M, reservoirSpatialVPL.M);

                if (IsReservoirValidVPL(reservoirSpatialVPL)) {
                    VPLReconnectionInfo vplSpatial = vplCache[reservoirSpatialVPL.vplIndex];
                    reservoirVPL = CombineReservoirsVPL(reservoirVPL, reservoirSpatialVPL, vpl, vplSpatial, its, material, sampler, vpl);
                }
            }
        }

        if (IsReservoirValidVPL(reservoirVPL))
        {
            float3 radiance = getOccludedContributionFromVPL(its, material, vpl);

            //
            if (length(radiance) > 0.001) {
                if (outPixelUsage[reservoirVPL.vplIndex] != 5) {
                    outPixelUsage[reservoirVPL.vplIndex] = 5;         
                }
                // this needs to be solved better in the future
                mutationPixelIndices[reservoirVPL.vplIndex] = uint2(launchID);       
            }

            output = (radiance * reservoirVPL.lightWeight);
        } else {
            reservoirVPL = createEmptyReservoirVPL();
        }

        float3 albedo = inAlbedoAlpha[launchID].xyz;
        float3 lighting = output / max(albedo,0.001);

        storeReservoirVPL(reservoirVPL, launchID, launchSize, outReservoirVPL);
        outImage[launchID] = float4(lighting, 1.0);
    }
}
