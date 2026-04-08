import Defines;
import Materials;
import Lightsource;
import Random;
import Frame;
import BSDF;
import ColorSpace;
import Surface;
import raytracing;

float3 directRaycast(in IntersectionRecord its, in MaterialRecord material, inout Sampler sampler) {
    // Inputs: intersection, material, RNG. Output: direct lighting contribution.
    EmitterRecord emitterRecord = sampleRandomLightSourceWeighted(lightBuffer, its.position, sampler);

    bool occluded = isLightOccluded(its, emitterRecord);

    if (occluded) {
        return float3(0.0, 0.0, 0.0);
    }

    return getUnoccludedLightContribution(its, material, emitterRecord);
}

IntegrationStepQueryRecord simulateFirstBounce(inout Sampler sampler) {
    // Inputs: RNG. Output: first-bounce integration state.
    float2 launchID = (float2)DispatchRaysIndex().xy;        // Current pixel (x,y)
    float2 launchSize = (float2)DispatchRaysDimensions().xy; // Image size (width,height)

    IntersectionRecord its;

    RayDesc cameraRay = getInitialRay(launchID, launchSize);

    its.position = inPosition[int2(launchID)].xyz;
    its.normal = inNormal[int2(launchID)].xyz;
    its.frame = Frame(its.normal);
    its.direction = cameraRay.Direction;

    MaterialRecord material;
    material.albedo = inAlbedoAlpha[int2(launchID)].xyz;
    material.alpha = inAlbedoAlpha[int2(launchID)].w;
    //Incorrect but not needed
    material.normal = float3(0.0,0.0,1.0);
    material.roughness = inSpec[int2(launchID)].x;
    material.metalness = inSpec[int2(launchID)].y;
    material.ior = inSpec[int2(launchID)].z;

    float3 shadingNormal = inShadingNormal[int2(launchID)].xyz;

    if (dot(its.normal, its.direction) > 0.0) {
        its.normal *= -1.0;
        shadingNormal *= -1.0;
    }

    material.shadingFrame = Frame(shadingNormal);

    float3 directLight = directRaycast(its, material, sampler);
    
    BSDFQueryRecord record = constructBSDFRecord(its, material, false);

    float3 bsdf = samplePrincipled(record, material, sampler);
    // RayDesc indirectRay = getRay(its, its.frame.toWorld(record.wo));
    RayDesc indirectRay = getRay(its, record.woWorld);

    IntegrationStepQueryRecord integrationStep;

    integrationStep.directLight = directLight;
    integrationStep.indirectLight = float3(0.0);
    integrationStep.throughput = bsdf;
    integrationStep.indirectRay = indirectRay;

    return integrationStep;
}

//Direct Integrator
float3 integratorDirect(inout Sampler sampler) {
    // Inputs: RNG. Output: direct-only lighting for a pixel.
    IntegrationStepQueryRecord integrationStep = simulateFirstBounce(sampler);

    return integrationStep.directLight;
}

float RRProbability(in float3 throughput) {
    // Input: throughput. Output: Russian roulette continuation probability.
    float maxXY = max(throughput.x, throughput.y);
    float max = max(maxXY, throughput.z);

    return max;
}

float3 clipThroughput(in float3 throughput) {
    // Input: throughput. Output: clamped throughput for stability.
    float3 throughputClipped;
    throughputClipped.x = min(throughput.x, 0.99);
    throughputClipped.y = min(throughput.y, 0.99);
    throughputClipped.z = min(throughput.z, 0.99);

    return throughputClipped;
}

IntegrationStepQueryRecord integratorPT(inout Sampler sampler) {
    // Inputs: RNG. Output: path-traced radiance for a pixel.
    IntegrationStepQueryRecord integrationStep = simulateFirstBounce(sampler);

    uint depth = 1;

    while (true) {
        float RRProb = RRProbability(integrationStep.throughput);

        HitPayload payload;
        const uint rayFlags = 0;
        TraceRay(topLevelAS, rayFlags, 0xff, 0, 0, 0, integrationStep.indirectRay, payload);

        if (!payload.hit) {
            break;
        }

        IntersectionRecord its = getIntersectionInfo(payload, integrationStep.indirectRay);

        

        Material material = materials[its.materialIdx];
        MaterialRecord materialRecord = getMaterialProperties(material, textures, its.uv, its.frame);

        BSDFQueryRecord shaderRecord = constructBSDFRecord(its, materialRecord, false);

        float3 bsdf = samplePrincipled(shaderRecord, materialRecord, sampler);
        integrationStep.indirectRay = getRayInternal(its, shaderRecord.woWorld);

        float3 directLight = directRaycast(its, materialRecord, sampler);
        integrationStep.indirectLight += directLight * integrationStep.throughput;

        if (sampler.UniformFloat() >= RRProb) {
            break;
        }

        integrationStep.throughput *= ((bsdf) / (RRProb));
        depth++;
    }

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

        IntegrationStepQueryRecord outputRecord = integratorPT(sampler);
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
