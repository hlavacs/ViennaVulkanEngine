import IR_Bindings;
import IR_Defines;
import IR_VPL_Generation_Common;
import raytracing;
import Random;
import Surface;
import Defines;

[[vk::binding(0, 2)]]
RWTexture2D<float4> inNormal;

[[vk::binding(1, 2)]]
RWTexture2D<float4> inPosition;

[[vk::binding(2, 2)]]
RWStructuredBuffer<VPLShading> vplCacheShading;

[[vk::binding(3, 2)]]
RWStructuredBuffer<VPLLimits> limits;

[[vk::binding(4, 2)]]
RWStructuredBuffer<uint> indices;

[[vk::binding(5, 2)]]
StructuredBuffer<uint2> mutationPixelIndices;

[[vk::binding(6, 2)]]
RWTexture2D<float4> outImage;


[shader("raygeneration")]
void rgenMain()
{
    uint2 launchID = DispatchRaysIndex().xy;        // Current pixel (x,y)
    uint2 launchSize = DispatchRaysDimensions().xy; // Image size (width,height)

    uint2 ScreenSize = uint2(x_dimensions, y_dimensions);

    //maybe the seed isn't good in 1D
    uint linear = (uint)launchID.x * (uint)launchSize.x;
    uint pixelSeed = Hash(linear ^ (seed * 0x9E3779B9u));

    Sampler sampler = Sampler(pixelSeed);

    uint index = indices[launchID.x];

    if (launchID.x <= limits[0].replacmentLimit) {

        VPLReconnectionInfo vpl;
        VPLShading shadingInfo;

        // if (generateVPL(sampler, ScreenSize, vpl, shadingInfo)) {
        // if (generateVPLMutation(sampler, ScreenSize, vpl, shadingInfo, launchID.x)) {

        if (generateVPLMutation(sampler, ScreenSize, vpl, shadingInfo, launchID.x)) {
            vplCache[index] = vpl;
            vplCacheShading[index] = shadingInfo;
        } else {
            vpl.age = 0;
            vpl.normal = float3(1.0);
            vpl.weightedRadiance = float3(0.0);
            vpl.position = float3(-INFINITE);

            shadingInfo.albedo = float3(0.0);
            shadingInfo.radiance = float3(0.0);

            vplCache[index] = vpl;
            vplCacheShading[index] = shadingInfo;
        }
    } else {
        vplCache[index].age++;
    }
}

bool generateVPL(inout Sampler sampler, uint2 launchSize, out VPLReconnectionInfo vpl, out VPLShading shadingInfo) {
    
    //quadruple check if this is correct. A error here could explain why I get the strange behaviour!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    //maybe there is a strong correlation between sample direction and pixel choosen
    uint x = sampler.UniformUIntFast(0, launchSize.x - 1);
    uint y = sampler.UniformUIntFast(0, launchSize.y - 1);

    uint2 launchID = uint2(x, y);

    float3 position = inPosition[int2(launchID)].xyz;
    float3 normal = inNormal[int2(launchID)].xyz;

    vpl.age = 0;
    vpl.normal = float3(1.0);
    vpl.position = float3(-INFINITE);
    vpl.weightedRadiance = float3(0.0);
    shadingInfo.albedo = float3(0.0);
    shadingInfo.radiance = float3(0.0);

    if (dot(normal, normal) < 0.001 || any(isnan(normal)) || any(isnan(position))) {
        return false;
    }

    if (isSkyPixel(position)) {
        return false;
    }

    IntersectionRecord its;
    getInitialSurfaceData(launchID, launchSize, position, normal, its);
    
    RayDesc ray = generateVPLRay(its, sampler);

    if (!createVPL(its, ray, vpl, shadingInfo)) {
        return false;
    }
    shadeVPL(vpl, shadingInfo, sampler, 24);
    setWeightedRadiance(vpl, shadingInfo, its);

    return true;
}

bool generateVPLMutation(inout Sampler sampler, uint2 launchSize, out VPLReconnectionInfo vpl, out VPLShading shadingInfo, uint vplIndex) {

    // quadruple check if this is correct. A error here could explain why I get the strange behaviour!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    // maybe there is a strong correlation between sample direction and pixel choosen

    bool useMutation = sampler.UniformFloat() < 0.5;

    if (limits[0].reproductionLimit == vplCacheSize) {
        useMutation = false;
    }

    uint2 launchID;
    uint vplMutationIndex =  0;

    if (useMutation) {
        // pick a good performing vpl
        vplMutationIndex = sampler.UniformUIntFast(limits[0].reproductionLimit, vplCacheSize - 1);
        launchID = mutationPixelIndices[vplMutationIndex];
    } else {
        uint x = sampler.UniformUIntFast(0, launchSize.x - 1);
        uint y = sampler.UniformUIntFast(0, launchSize.y - 1);

        launchID = uint2(x, y);
    }

    /*
    if (useMutation) {
        outImage[launchID] = float4(20.0, 0.0, 0.0, 0.0);
    } else {
        outImage[launchID] = float4(0.0, 0.0, 20.0, 0.0);
    }
        */
    

    float3 position = inPosition[int2(launchID)].xyz;
    float3 normal = inNormal[int2(launchID)].xyz;

    vpl.age = 0;
    vpl.normal = float3(1.0);
    vpl.position = float3(-INFINITE);
    vpl.weightedRadiance = float3(0.0);
    shadingInfo.albedo = float3(0.0);
    shadingInfo.radiance = float3(0.0);

    if (dot(normal, normal) < 0.001 || any(isnan(normal)) || any(isnan(position))) {
        return false;
    }

    if (isSkyPixel(position)) {
        return false;
    }

    IntersectionRecord its;
    getInitialSurfaceData(launchID, launchSize, position, normal, its);

    RayDesc ray;

    if (useMutation) {
        float mutation = 0.01;
        VPLReconnectionInfo mutationVPL = vplCache[vplMutationIndex];     
        ray = mutateVPLRay(its, mutationVPL, sampler, mutation);
    } else {
        ray = generateVPLRay(its, sampler);
    }
    

    if (!createVPL(its, ray, vpl, shadingInfo)) {
        return false;
    }
    shadeVPL(vpl, shadingInfo, sampler, 24);
    setWeightedRadiance(vpl, shadingInfo, its);

    return true;
}