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
import RestirDI;
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
RWTexture2D<float4> inSpec;

[[vk::binding(3, 2)]]
RWTexture2D<float4> inPosition;

[[vk::binding(4, 2)]]
RWTexture2D<float4> inShadingNormal;

[[vk::binding(5, 2)]]
RWTexture2D<float4> inReprojectionError;

[[vk::binding(6, 2)]]
RWStructuredBuffer<Reservoir> inReservoir;

[[vk::binding(7, 2)]]
RWStructuredBuffer<Reservoir> outReservoir;

[[vk::binding(8, 2)]]
RWStructuredBuffer<ReservoirVPL> inReservoirVPL;

[[vk::binding(9, 2)]]
RWStructuredBuffer<ReservoirVPL> outReservoirVPL;

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

    const uint CANDIDATE_COUNT_GI = 16;
    const uint MAX_TEMPORAL_INFLUENCE_GI = 20;

    if (isSkyPixel(position)) {
        Reservoir reservoir = createEmptyReservoir();
        ReservoirVPL reservoirLVC = createEmptyReservoirVPL();
        storeReservoir(reservoir, launchID, launchSize, outReservoir);
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

        Reservoir reservoir = createEmptyReservoir();

        // initial selection of 1 light of M
        for (uint i = 0; i < CANDIDATE_COUNT; i++)
        {
            uint lightIndex = sampler.UniformUIntFast(0, lightCount - 1);

            float4 samplePosition = float4(0.0);

            p_hat = sampleUnoccludedLightRadiance(lightIndex, its, material, sampler, samplePosition);

            float w = p_hat / lightPdf;

            UpdateReservoir(reservoir, lightIndex, samplePosition, w, 1, sampler);
        }

        if (IsReservoirValid(reservoir))
        {
            // Update weight with new radiance with visibility term!
            float p_hat = length(revaluateOccludedLightRadiance(reservoir, its, material));
            reservoir.lightWeight = p_hat > 0.0 ? (1.0 / max(p_hat, 0.001)) * reservoir.W_sum / reservoir.M : 0.0;
        } else {
            reservoir = createEmptyReservoir();
        }

        //reject history if reprojection error is too big
        //better system needed in the future
        if (inReprojectionError[launchID].x < 0.5) {
            Reservoir temporalReservoir = readReservoir(launchID, launchSize, inReservoir);

            // restrict influence from past samples.
            // this may not be optimal, the real time raytracing engine handles this diffrently
            temporalReservoir.M = min(MAX_TEMPORAL_INFLUENCE * reservoir.M, temporalReservoir.M);

            reservoir = CombineReservoirs(reservoir, temporalReservoir, its, material, sampler);
        }

        //GI
        ReservoirVPL reservoirVPL = createEmptyReservoirVPL();
        VPLReconnectionInfo vpl;

        // initial selection of 1 light vertex of M
        for (uint i = 0; i < CANDIDATE_COUNT_GI; i++)
        {
            uint vplIndex = sampler.UniformUIntFast(0, vplCacheSize - 1);
            VPLReconnectionInfo vplCandidate = vplCache[vplIndex];

            p_hat = length(getUnoccludedContributionFromVPL(its, material, vplCandidate));

            float w = p_hat;

            if (UpdateReservoirVPL(reservoirVPL, vplIndex, w, 1, sampler)) {
                vpl = vplCandidate;
            }
        }

        if (IsReservoirValidVPL(reservoirVPL))
        {
            // Update weight with new radiance with visibility term!
            float p_hat = length(getOccludedContributionFromVPL(its, material, vpl));

            reservoirVPL.lightWeight = p_hat > 0.0 ? (1.0 / max(p_hat, 0.001)) * reservoirVPL.W_sum / reservoirVPL.M : 0.0;

            
        } else {
            reservoirVPL = createEmptyReservoirVPL();
        }

        // reject history if reprojection error is too big
        // better system needed in the future
        if (inReprojectionError[launchID].x < 0.5) {
            ReservoirVPL temporalReservoirVPL = readReservoirVPL(launchID, launchSize, inReservoirVPL);

            // if temporal light vertex has age 0 it was newly generated invalidating the temporal history
            VPLReconnectionInfo vplTemporal = vplCache[temporalReservoirVPL.vplIndex];

            if (vplTemporal.age != 0) {
                // restrict influence from past samples.
                temporalReservoirVPL.M = min(MAX_TEMPORAL_INFLUENCE_GI * reservoirVPL.M, temporalReservoirVPL.M);
                reservoirVPL = CombineReservoirsVPL(reservoirVPL, temporalReservoirVPL, vpl, vplTemporal, its, material, sampler, vpl);
            } else {
                // find  best matching valid history from  the neighbours
                float bestDistance = 1000.0;
                ReservoirVPL bestMatchReservoir;
                VPLReconnectionInfo bestVPL;
                bool foundValidReservoir = false;

                for (int x = -1; x <= 1; x++) {
                    for (int y = -1; y <= 1; y++) {
                        int2 offset = int2(x, y);
                        uint2 readID = launchID + offset;

                        float3 positionNeighbour = inPosition[int2(readID)].xyz;
                        float distance = length(positionNeighbour - position);

                        if (distance < bestDistance) {
                            ReservoirVPL neighbourReservoirVPL = readReservoirVPL(readID, launchSize, inReservoirVPL);
                            // if temporal light vertex has age 0 it was newly generated invalidating the temporal history
                            VPLReconnectionInfo vplNeighbour = vplCache[neighbourReservoirVPL.vplIndex];

                            if (vplNeighbour.age != 0) {
                                bestMatchReservoir = neighbourReservoirVPL;
                                bestVPL = vplNeighbour;
                                bestMatchReservoir.M = min(MAX_TEMPORAL_INFLUENCE_GI * reservoirVPL.M, bestMatchReservoir.M);
                                bestDistance = distance;
                                foundValidReservoir = true;
                            }
                        }
                    } 
                }

                if (foundValidReservoir) {
                    reservoirVPL = CombineReservoirsVPL(reservoirVPL, bestMatchReservoir, vpl, bestVPL, its, material, sampler, vpl);
                }
            }   
        }

        storeReservoir(reservoir, launchID, launchSize, outReservoir);
        storeReservoirVPL(reservoirVPL, launchID, launchSize, outReservoirVPL);
    }
}
