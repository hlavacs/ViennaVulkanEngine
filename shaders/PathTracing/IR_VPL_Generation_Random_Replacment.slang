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


[shader("raygeneration")]
void rgenMain()
{
    uint2 launchID = DispatchRaysIndex().xy;        // Current pixel (x,y)
    uint2 launchSize = DispatchRaysDimensions().xy; // Image size (width,height)

    uint2 ScreenSize = uint2(x_dimensions, y_dimensions);

    uint linear = (uint)launchID.x + (uint)launchID.y * (uint)launchSize.x;
    uint pixelSeed = Hash(linear ^ (seed * 0x9E3779B9u));

    Sampler sampler = Sampler(pixelSeed);

    if (sampler.UniformFloat() < 0.01 || isFirstFrame == 1) {

        VPLReconnectionInfo vpl;
        VPLShading shadingInfo;

        if (generateVPL(sampler, ScreenSize, vpl, shadingInfo)) {
            vplCache[launchID.x] = vpl;
            vplCacheShading[launchID.x] = shadingInfo;
        } else {
            vpl.age = 0;
            vpl.normal = float3(1.0);
            vpl.weightedRadiance = float3(0.0);
            vpl.position = float3(-INFINITE);

            shadingInfo.albedo = float3(0.0);
            shadingInfo.radiance = float3(0.0);

            vplCache[launchID.x] = vpl;
            vplCacheShading[launchID.x] = shadingInfo;
        }
        


    } else {
        vplCache[launchID.x].age++;
    }
}

bool generateVPL(inout Sampler sampler, uint2 launchSize, out VPLReconnectionInfo vpl, out VPLShading shadingInfo) {
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
    shadeVPL(vpl, shadingInfo, sampler, 4);
    setWeightedRadiance(vpl, shadingInfo, its);

    return true;
}