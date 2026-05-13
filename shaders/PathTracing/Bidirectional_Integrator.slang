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


float3 groundTruthLightRadiance(in uint lightIndex, in IntersectionRecord its, in MaterialRecord material, inout Sampler sampler) {
    LightSource light = lightBuffer[lightIndex];
    EmitterRecord record = sampleLightSource(light, its.position, sampler);

    bool occluded = isLightOccluded(its, record);

    if (occluded) {
        return 0.0;
    }

    return getUnoccludedLightContribution(its, material, record);
}

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

//Direct Integrator
IntegrationStepQueryRecord integratorDirect(inout Sampler sampler) {

    int2 launchID = (int2)DispatchRaysIndex().xy;        // Current pixel (x,y)
    int2 launchSize = (int2)DispatchRaysDimensions().xy; // Image size (width,height)

    IntersectionRecord its;
    MaterialRecord material;

    getInitialSurfaceData(launchID,launchSize, its, material);

    float3 radiance = float3(0.0);
    float pdf = 1.0 / lightCount;

    for(uint i = 0; i < lightCount; i++){
        radiance += (groundTruthLightRadiance(i, its, material, sampler) / pdf);
    }

    IntegrationStepQueryRecord integrationStep;

    integrationStep.directLight = radiance / (float)lightCount;
    integrationStep.indirectLight = float3(0.0);
    integrationStep.throughput = float3(0.0);
    //integrationStep.indirectRay = indirectRay;

    return integrationStep;
}

// Indirect Integrator
IntegrationStepQueryRecord integratorBidirectional(inout Sampler sampler, ) {

    int2 launchID = (int2)DispatchRaysIndex().xy;        // Current pixel (x,y)
    int2 launchSize = (int2)DispatchRaysDimensions().xy; // Image size (width,height)

    IntersectionRecord its;
    MaterialRecord material;

    getInitialSurfaceData(launchID, launchSize, its, material);

    float pdf = 1.0 / float(lightVertexCacheSize);

    uint sampleCount = 10;
    float3 radiance = 0.0;

    for (uint i = 0; i < sampleCount; i++) {
        uint index = sampler.UniformUIntFast(0, lightVertexCacheSize - 1);
        // do not devide by the pdf
        // radiance += getOccludedLightContributionFromConnection(its, material, lightVertex) / pdf;
        radiance += getOccludedLightContributionFromConnection(its, material, index);
    }
    
    IntegrationStepQueryRecord integrationStep;

    integrationStep.directLight = float3(0.0);
    integrationStep.indirectLight = radiance / sampleCount;
    integrationStep.throughput = float3(0.0);
    // integrationStep.indirectRay = indirectRay;

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

        IntegrationStepQueryRecord outputDirectRecord = integratorDirect(sampler);
        IntegrationStepQueryRecord outputIndirectRecord = integratorBidirectional(sampler);
        // outImage[int2(launchID)] = float4(LinearToSRGB(outputColor), 1.0);
        float3 albedo = inAlbedoAlpha[int2(launchID)].xyz;
        //works for now, but terrible with reprojection errors

        float3 outputColor = outputDirectRecord.directLight + outputIndirectRecord.indirectLight;

        
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

// Closest hit shader - called when ray hits geometry
[shader("closesthit")]
void rchitMain(inout HitPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    // Inputs: hit attributes, payload (inout). Output: filled payload for intersection.
    payload.hit = true;
    payload.distance = RayTCurrent();
    payload.instanceIdx = InstanceIndex();
    payload.triangleIdx = PrimitiveIndex();
    payload.barycentrics = float3(attr.barycentrics.x, attr.barycentrics.y, 1.0 - attr.barycentrics.x - attr.barycentrics.y);

    //uint firstIndex = InstanceID(); This is how to access the costom index

    payload.color = float3(1, 0, 0); // Red color
}

// Miss shader - called when ray doesn't hit any geometry
[shader("miss")]
void rmissMain(inout HitPayload payload)
{
    // Inputs: payload (inout). Output: miss payload values.
    // Set background color when ray misses all geometry
    // In a full implementation, this could sample from:
    // - Skybox textures
    // - Procedural sky gradients
    // - Environment maps
    payload.hit = false;
    payload.distance = INFINITE;
    payload.color = float3(0.5, 0.5, 0.5); // Gray background
}
