import Defines;
import Materials;
import Lightsource;
import Random;
import Frame;
import BSDF;
import ColorSpace;
import Surface;
import raytracing;
import IR_VPL_Evaluation;
import IR_VPL_Generation_Common;
import IR_Common;
import IR_Defines;

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

VPLReconnectionInfo generateVPL(inout Sampler sampler, uint2 launchSize, uint2 launchID) {
    float3 position = inPosition[int2(launchID)].xyz;
    float3 normal = inNormal[int2(launchID)].xyz;

    IntersectionRecord its;
    getInitialSurfaceData(launchID, launchSize, position, normal, its);

    RayDesc ray = generateVPLRay(its, sampler);

    VPLReconnectionInfo vpl;
    vpl.age = 0;
    VPLShading shadingInfo;

    createVPL(its, ray, vpl, shadingInfo);
    shadeVPL(vpl, shadingInfo, sampler, 4);
    setWeightedRadiance(vpl, shadingInfo, its);

    return vpl;
}

//Direct Integrator
IntegrationStepQueryRecord integratorDirect(inout Sampler sampler) {

    int2 launchID = (int2)DispatchRaysIndex().xy;        // Current pixel (x,y)
    int2 launchSize = (int2)DispatchRaysDimensions().xy; // Image size (width,height)

    IntersectionRecord its;
    MaterialRecord material;

    getInitialSurfaceData(launchID,launchSize, its, material);

    float3 radiance = 0.0;
    uint sampleCount = 10;

    for (uint i = 0; i < sampleCount; i++) {
        VPLReconnectionInfo vpl = generateVPL(sampler, launchSize, launchID);
        radiance += getOccludedContributionFromVPL(its, material, vpl);
    }
    

    IntegrationStepQueryRecord integrationStep;

    integrationStep.directLight = radiance / sampleCount;
    integrationStep.indirectLight = float3(0.0);
    integrationStep.throughput = float3(0.0);
    //integrationStep.indirectRay = indirectRay;

    return integrationStep;
}

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



// Ray generation shader - entry point for each pixel
[shader("raygeneration")]
void rgenMain()
{
    // Output: writes the final color to outImage.
    
    // Get the current pixel coordinates and image dimensions
    float2 launchID = (float2)DispatchRaysIndex().xy;     // Current pixel (x,y)
    float2 launchSize = (float2)DispatchRaysDimensions().xy; // Image size (width,height)
    
    float3 position = inPosition[int2(launchID)].xyz;

    if (isSkyPixel(position)) {
        outImage[int2(launchID)] = float4(0.0, 0.0, 0.0, 1.0);
    } else {
        uint linear = (uint)launchID.x + (uint)launchID.y * (uint)launchSize.x;
        uint pixelSeed = Hash(linear ^ (seed * 0x9E3779B9u));

        Sampler sampler = Sampler(pixelSeed);

        IntegrationStepQueryRecord outputRecord = integratorDirect(sampler);
        // outImage[int2(launchID)] = float4(LinearToSRGB(outputColor), 1.0);
        float3 albedo = inAlbedoAlpha[int2(launchID)].xyz;
        //works for now, but terrible with reprojection errors

        float3 outputColor = outputRecord.directLight + outputRecord.indirectLight;

        
        //this works for now but is likley to break with slight reprojection errors.
        //in the end I have to make sure to seperate everything that gets multiplied with the albedo
        //aka direct diffuse + dirrect metallic specular will be in one render target.
        //indirect lighting + direct dielectic specular in a second target
        //maybe splitting between specular and diffuse also works. In that case the diffuse would still have to be devided,
        //by albedo. But indirect has less tendency to be large when albedo is small because indirect is already weighted with throughput 
        //not sure about the math it might actually be garanteed to not be large.
        float3 lighting = outputColor / max(albedo,0.001);
        outImage[int2(launchID)] = float4(lighting, 1.0);    
    }
}