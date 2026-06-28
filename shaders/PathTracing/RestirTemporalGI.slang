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
import ReservoirGI;
import RestirDI;
import RestirGI;

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
RWTexture2D<float4> inPositionPrevious;

[[vk::binding(7, 2)]]
RWStructuredBuffer<Reservoir> inReservoir;

[[vk::binding(8, 2)]]
RWStructuredBuffer<Reservoir> outReservoir;

[[vk::binding(9, 2)]]
RWStructuredBuffer<ReservoirGI> inReservoirGI;

[[vk::binding(10, 2)]]
RWStructuredBuffer<ReservoirGI> outReservoirGI;



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
    const uint MAX_TEMPORAL_INFLUENCE_GI = 20;
    const uint MAX_AGE = 25;

    float selectedTargetPdf = 0.0;
    float piSum = 0.0;

    if (isSkyPixel(position)) {
        Reservoir reservoir = createEmptyReservoir();
        ReservoirGI reservoirGI = createEmptyReservoirGI();
        storeReservoir(reservoir, launchID, launchSize, outReservoir);
        storeReservoirGI(reservoirGI, launchID, launchSize, outReservoirGI);
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
        ReservoirGI reservoirGI = createEmptyReservoirGI();

        // initial selection of 1 light of M
        for (uint i = 0; i < CANDIDATE_COUNT; i++)
        {
            uint lightIndex = sampler.UniformUIntFast(0, lightCount - 1);

            float4 samplePosition = float4(0.0);

            p_hat = sampleUnoccludedLightRadiance(lightIndex, its, material, sampler, samplePosition);

            float w = p_hat / lightPdf;

            UpdateReservoir(reservoir, lightIndex, samplePosition, w, 1, sampler);
        }

        // initial generation of GI sample
        // ReservoirGI initialGISample = createInitialReservoirGI(its, material, sampler);
        ReservoirGI initialGISample = createInitialReservoirGI(its, material, sampler);

        if (IsReservoirValidGI(initialGISample)) {
            float targetPDF = getApproximateRadianceFromReservoir(initialGISample, its, material);
            //float targetPDF = length(getGroundTruthRadianceFromReservoir(initialGISample, its, material));
            selectedTargetPdf = targetPDF;
            piSum += targetPDF * initialGISample.M;
            CombineReservoirsGI(reservoirGI, initialGISample, targetPDF, sampler);
        }
        


        //ser lightweight of DI reservoir using visibility
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
        if (inReprojectionError[launchID].x < 0.1) {
            Reservoir temporalReservoir = readReservoir(launchID, launchSize, inReservoir);
            // restrict influence from past samples.
            // this may not be optimal, the real time raytracing engine handles this diffrently
            temporalReservoir.M = min(MAX_TEMPORAL_INFLUENCE * reservoir.M, temporalReservoir.M);
            reservoir = CombineReservoirs(reservoir, temporalReservoir, its, material, sampler);

            // GI
            ReservoirGI temporalReservoirGI = readReservoirGI(launchID, launchSize, inReservoirGI);

            
            if (IsReservoirValidGI(temporalReservoirGI) && temporalReservoirGI.age < MAX_AGE) {
                float jacobian = getJacobian(inPosition[launchID].xyz, inPositionPrevious[launchID].xyz, temporalReservoirGI.position, temporalReservoirGI.normal);

                if (validateJacobian(jacobian)) {

                    temporalReservoirGI.W_sum *= jacobian;
                    temporalReservoirGI.M = min(MAX_TEMPORAL_INFLUENCE_GI, temporalReservoirGI.M);
                    temporalReservoirGI.age++;
                    
                    float targetPDF = getApproximateRadianceFromReservoir(temporalReservoirGI, its, material);
                    if (CombineReservoirsGI(reservoirGI, temporalReservoirGI, targetPDF, sampler)) {
                        selectedTargetPdf = targetPDF;
                    }
                    piSum += targetPDF * temporalReservoirGI.M;
                }
            }
        }

        //float normalizationNumerator = selectedTargetPdf;
        //float normalizationDenominator = piSum * selectedTargetPdf;

        float normalizationNumerator = 1.0;
        float normalizationDenominator = selectedTargetPdf * float(reservoirGI.M);

        finalizeGIResampling(reservoirGI, normalizationNumerator, normalizationDenominator);

        storeReservoir(reservoir, launchID, launchSize, outReservoir);
        storeReservoirGI(reservoirGI, launchID, launchSize, outReservoirGI);
    }
}

//this is way slower for some reason!!!!!

/*
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
    const uint MAX_TEMPORAL_INFLUENCE_GI = 20;
    const uint MAX_AGE = 25;


    if (isSkyPixel(position)) {
        Reservoir reservoir = createEmptyReservoir();
        ReservoirGI reservoirGI = createEmptyReservoirGI();
        storeReservoir(reservoir, launchID, launchSize, outReservoir);
        storeReservoirGI(reservoirGI, launchID, launchSize, outReservoirGI);
    } else {
        uint linear = (uint)launchID.x + (uint)launchID.y * (uint)launchSize.x;
        uint pixelSeed = Hash(linear ^ (seed * 0x9E3779B9u));

        Sampler sampler = Sampler(pixelSeed);

        float lightPdf = 1.0 / float(lightCount);
        float p_hat = 0;

        MaterialRecord material;
        IntersectionRecord its;

        getInitialSurfaceData(launchID, launchSize, its, material);

        if (illuminationDomain == 0 || illuminationDomain == 1) {
            // Restir DI
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

            // ser lightweight of DI reservoir using visibility
            if (IsReservoirValid(reservoir))
            {
                // Update weight with new radiance with visibility term!
                float p_hat = length(revaluateOccludedLightRadiance(reservoir, its, material));
                reservoir.lightWeight = p_hat > 0.0 ? (1.0 / max(p_hat, 0.001)) * reservoir.W_sum / reservoir.M : 0.0;
            } else {
                reservoir = createEmptyReservoir();
            }

            if (inReprojectionError[launchID].x < 0.1) {
                // Restir DI
                Reservoir temporalReservoir = readReservoir(launchID, launchSize, inReservoir);
                // restrict influence from past samples.
                // this may not be optimal, the real time raytracing engine handles this diffrently
                temporalReservoir.M = min(MAX_TEMPORAL_INFLUENCE * reservoir.M, temporalReservoir.M);
                reservoir = CombineReservoirs(reservoir, temporalReservoir, its, material, sampler);
            }
            storeReservoir(reservoir, launchID, launchSize, outReservoir);
        }

        if (illuminationDomain == 0 || illuminationDomain == 2) {
            // Restir GI
            ReservoirGI reservoirGI = createEmptyReservoirGI();

            float selectedTargetPdf = 0.0;
            float piSum = 0.0;

            // initial generation of GI sample
            // ReservoirGI initialGISample = createInitialReservoirGI(its, material, sampler);
            ReservoirGI initialGISample = createInitialReservoirGI(its, material, sampler);

            if (IsReservoirValidGI(initialGISample)) {
                float targetPDF = getApproximateRadianceFromReservoir(initialGISample, its, material);
                //float targetPDF = length(getGroundTruthRadianceFromReservoir(initialGISample, its, material));
                selectedTargetPdf = targetPDF;
                piSum += targetPDF * initialGISample.M;
                CombineReservoirsGI(reservoirGI, initialGISample, targetPDF, sampler);
            }

            //reject history if reprojection error is too big
            //better system needed in the future
            if (inReprojectionError[launchID].x < 0.1) {
                ReservoirGI temporalReservoirGI = readReservoirGI(launchID, launchSize, inReservoirGI);

                if (IsReservoirValidGI(temporalReservoirGI) && temporalReservoirGI.age < MAX_AGE) {
                    float jacobian = getJacobian(inPosition[launchID].xyz, inPositionPrevious[launchID].xyz, temporalReservoirGI.position, temporalReservoirGI.normal);

                    if (validateJacobian(jacobian)) {

                        temporalReservoirGI.W_sum *= jacobian;
                        temporalReservoirGI.M = min(MAX_TEMPORAL_INFLUENCE_GI, temporalReservoirGI.M);
                        temporalReservoirGI.age++;

                        float targetPDF = getApproximateRadianceFromReservoir(temporalReservoirGI, its, material);
                        if (CombineReservoirsGI(reservoirGI, temporalReservoirGI, targetPDF, sampler)) {
                            selectedTargetPdf = targetPDF;
                        }
                        piSum += targetPDF * temporalReservoirGI.M;
                    }
                }
            }

            //float normalizationNumerator = selectedTargetPdf;
            //float normalizationDenominator = piSum * selectedTargetPdf;

            float normalizationNumerator = 1.0;
            float normalizationDenominator = selectedTargetPdf * float(reservoirGI.M);

            finalizeGIResampling(reservoirGI, normalizationNumerator, normalizationDenominator);
            storeReservoirGI(reservoirGI, launchID, launchSize, outReservoirGI);
        }
    }
}

*/

/*
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
import ReservoirGI;
import RestirDI;
import RestirGI;
import RestirTemporal;

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
RWTexture2D<float4> inPositionPrevious;

[[vk::binding(7, 2)]]
RWStructuredBuffer<Reservoir> inReservoir;

[[vk::binding(8, 2)]]
RWStructuredBuffer<Reservoir> outReservoir;

[[vk::binding(9, 2)]]
RWStructuredBuffer<ReservoirGI> inReservoirGI;

[[vk::binding(10, 2)]]
RWStructuredBuffer<ReservoirGI> outReservoirGI;



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

    const uint MAX_TEMPORAL_INFLUENCE_GI = 20;
    const uint MAX_AGE = 25;

    float selectedTargetPdf = 0.0;
    float piSum = 0.0;

    if (isSkyPixel(position)) {
        storeEmptyReservoir(launchID, launchSize, outReservoir);
        ReservoirGI reservoirGI = createEmptyReservoirGI();
        storeReservoirGI(reservoirGI, launchID, launchSize, outReservoirGI);
    } else {
        uint linear = (uint)launchID.x + (uint)launchID.y * (uint)launchSize.x;
        uint pixelSeed = Hash(linear ^ (seed * 0x9E3779B9u));

        Sampler sampler = Sampler(pixelSeed);

        MaterialRecord material;
        IntersectionRecord its;

        getInitialSurfaceData(launchID, launchSize, its, material);

        if (illuminationDomain == 0 || illuminationDomain == 1) {
            restirTemporalDI(launchID, launchSize, sampler, material, its, inReprojectionError, inReservoir, outReservoir);
        }

        if (illuminationDomain == 0 || illuminationDomain == 2) {
            ReservoirGI reservoirGI = createEmptyReservoirGI();

            // initial generation of GI sample
            // ReservoirGI initialGISample = createInitialReservoirGI(its, material, sampler);
            ReservoirGI initialGISample = createInitialReservoirGI(its, material, sampler);

            if (IsReservoirValidGI(initialGISample)) {
                float targetPDF = getApproximateRadianceFromReservoir(initialGISample, its, material);
                //float targetPDF = length(getGroundTruthRadianceFromReservoir(initialGISample, its, material));
                selectedTargetPdf = targetPDF;
                piSum += targetPDF * initialGISample.M;
                CombineReservoirsGI(reservoirGI, initialGISample, targetPDF, sampler);
            }

            //reject history if reprojection error is too big
            //better system needed in the future
            if (inReprojectionError[launchID].x < 0.1) {
                // GI
                ReservoirGI temporalReservoirGI = readReservoirGI(launchID, launchSize, inReservoirGI);


                if (IsReservoirValidGI(temporalReservoirGI) && temporalReservoirGI.age < MAX_AGE) {
                    float jacobian = getJacobian(inPosition[launchID].xyz, inPositionPrevious[launchID].xyz, temporalReservoirGI.position, temporalReservoirGI.normal);

                    if (validateJacobian(jacobian)) {

                        temporalReservoirGI.W_sum *= jacobian;
                        temporalReservoirGI.M = min(MAX_TEMPORAL_INFLUENCE_GI, temporalReservoirGI.M);
                        temporalReservoirGI.age++;

                        float targetPDF = getApproximateRadianceFromReservoir(temporalReservoirGI, its, material);
                        if (CombineReservoirsGI(reservoirGI, temporalReservoirGI, targetPDF, sampler)) {
                            selectedTargetPdf = targetPDF;
                        }
                        piSum += targetPDF * temporalReservoirGI.M;
                    }
                }
            }

            float normalizationNumerator = 1.0;
            float normalizationDenominator = selectedTargetPdf * float(reservoirGI.M);

            finalizeGIResampling(reservoirGI, normalizationNumerator, normalizationDenominator);

            storeReservoirGI(reservoirGI, launchID, launchSize, outReservoirGI);
        }
    }
}

*/