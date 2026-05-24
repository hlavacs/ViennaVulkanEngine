import Defines;
import Materials;
import Lightsource;
import Random;
import Frame;
import BSDF;
import ColorSpace;
import Surface;
import raytracing;
import Bidrirectional_Raytracing;
import Reservoir;
import RestirDI;
import ReservoirLVC;
import RestirLVC;
import LightVertex;

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
RWTexture2D<float4> outImage;

[[vk::binding(6, 2)]]
RWStructuredBuffer<Reservoir> inReservoir;

[[vk::binding(7, 2)]]
RWStructuredBuffer<Reservoir> outReservoir;

[[vk::binding(8, 2)]]
RWStructuredBuffer<ReservoirLVC> inReservoirLVC;

[[vk::binding(9, 2)]]
RWStructuredBuffer<ReservoirLVC> outReservoirLVC;




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

    const uint MAX_SPATIAL_INFLUENCE = 10;
    const uint SPATIAL_SAMPLES = 6;
    const uint SAMPLE_RADIUS = 4;

    const uint MAX_SPATIAL_INFLUENCE_GI = 10;
    const uint SPATIAL_SAMPLES_GI = 12;
    const uint SAMPLE_RADIUS_GI = 12;

    if (isSkyPixel(position)) {
        Reservoir reservoir = createEmptyReservoir();
        ReservoirLVC reservoirLVC = createEmptyReservoirLVC();
        storeReservoir(reservoir, launchID, launchSize, outReservoir);
        storeReservoirLVC(reservoirLVC, launchID, launchSize, outReservoirLVC);
    } else {
        uint linear = (uint)launchID.x + (uint)launchID.y * (uint)launchSize.x;
        uint pixelSeed = Hash(linear ^ (seed * 0x9E3779B9u));

        Sampler sampler = Sampler(pixelSeed);

        float lightPdf = 1.0 / float(lightCount);
        float p_hat = 0;

        MaterialRecord material;
        IntersectionRecord its;

        getInitialSurfaceData(launchID, launchSize, its, material);

        Reservoir reservoir = readReservoir(launchID, launchSize, inReservoir);

        float3 normal = inNormal[launchID].xyz;
        float3 position = inPosition[launchID].xyz;

        float3 output = float3(0.0);

        for (uint i = 0; i < SPATIAL_SAMPLES; i++) {
            int x = sampler.UniformUIntFast(0, 2 * SAMPLE_RADIUS) - SAMPLE_RADIUS;
            int y = sampler.UniformUIntFast(0, 2 * SAMPLE_RADIUS) - SAMPLE_RADIUS;

            int2 offset = int2(x, y);

            int2 readID = launchID + offset;

            float3 normalSpatial = inNormal[readID].xyz;
            float3 positionSpatial = inPosition[readID].xyz;

            if (dot(normal, normalSpatial) >= 0.9 && length(positionSpatial - position) < 0.25) {
                Reservoir reservoirSpatial = readReservoir(readID, launchSize, inReservoir);

                // restrict influence from past samples.
                // this may not be optimal, the real time raytracing engine handles this diffrently
 
                reservoirSpatial.M = min(MAX_SPATIAL_INFLUENCE * reservoir.M, reservoirSpatial.M);

                if(IsReservoirValid(reservoirSpatial)){
                    reservoir = CombineReservoirs(reservoir, reservoirSpatial, its, material, sampler);
                }
                
            }     
        }

        if (IsReservoirValid(reservoir))
        {
            float3 radiance = revaluateOccludedLightRadiance(reservoir, its, material);
            //float3 radiance = resampleOccludedLightRadiance(reservoir, its, material, sampler);

            //output = radiance * reservoir.lightWeight;
            output = clamp(output, 0.0, 1000.0);
            float p_hat = length(radiance);
        } else {
            reservoir = createEmptyReservoir();
        }


        //GI

        ReservoirLVC reservoirLVC = readReservoirLVC(launchID, launchSize, inReservoirLVC);

        for (uint i = 0; i < SPATIAL_SAMPLES_GI; i++) {
            int x = sampler.UniformUIntFast(0, 2 * SAMPLE_RADIUS_GI) - SAMPLE_RADIUS_GI;
            int y = sampler.UniformUIntFast(0, 2 * SAMPLE_RADIUS_GI) - SAMPLE_RADIUS_GI;

            int2 offset = int2(x, y);

            int2 readID = launchID + offset;

            float3 normalSpatial = inNormal[readID].xyz;
            float3 positionSpatial = inPosition[readID].xyz;

            if (dot(normal, normalSpatial) >= 0.9 && length(positionSpatial - position) < 0.25) {
                ReservoirLVC reservoirSpatialLVC = readReservoirLVC(readID, launchSize, inReservoirLVC);

                // restrict influence from past samples.
                // this may not be optimal, the real time raytracing engine handles this diffrently
                reservoirSpatialLVC.M = min(MAX_SPATIAL_INFLUENCE_GI * reservoirLVC.M, reservoirSpatialLVC.M);

                //reservoirSpatialLVC.M = min(MAX_SPATIAL_INFLUENCE_GI, reservoirSpatialLVC.M);

                if (IsReservoirValidLVC(reservoirSpatialLVC)) {
                    reservoirLVC = CombineReservoirsLVC(reservoirLVC, reservoirSpatialLVC, its, material, sampler);
                }
            }
        }

        if (IsReservoirValidLVC(reservoirLVC))
        {
            float3 radiance = getOccludedLightContributionFromConnection(its, material, reservoirLVC.LVCIndex);
            LightVertex lightVertex = lightVertexCache[reservoirLVC.LVCIndex];
            //output += (radiance * reservoirLVC.lightWeight) / lightVertex.selectionPdf;
            output += (radiance * reservoirLVC.lightWeight);
            //right now importance is not an atomic, so the addition will give false results, which does not lead to Bias, but can
            //lead to increased noise. It is advisable to seperate importance from the Light Vertex, since no local copies of atomics 
            //can be created.
            lightVertexCache[reservoirLVC.LVCIndex].importance += length(radiance) * reservoirLVC.lightWeight;



        } else {
            reservoirLVC = createEmptyReservoirLVC();
        }

        float3 albedo = inAlbedoAlpha[launchID].xyz;
        float3 lighting = output / max(albedo,0.001);

        storeReservoir(reservoir, launchID, launchSize, outReservoir);
        storeReservoirLVC(reservoirLVC, launchID, launchSize, outReservoirLVC);
        outImage[launchID] = float4(lighting, 1.0);
    }
}
