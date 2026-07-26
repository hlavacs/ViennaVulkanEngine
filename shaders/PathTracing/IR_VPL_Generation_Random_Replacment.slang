import IR_Bindings;
import IR_Defines;
import IR_VPL_Generation_Common;
import raytracing;
import Random;
import Surface;

[shader("raygeneration")]
void rgenMain()
{
    uint2 launchID = DispatchRaysIndex().xy;        // Current pixel (x,y)
    uint2 launchSize = DispatchRaysDimensions().xy; // Image size (width,height)

    uint linear = (uint)launchID.x + (uint)launchID.y * (uint)launchSize.x;
    uint pixelSeed = Hash(linear ^ (seed * 0x9E3779B9u));

    Sampler sampler = Sampler(pixelSeed);

    if (sampler.UniformFloat() < 0.1 || isFirstFrame == 1) {
        VPLReconnectionInfo vpl = generateVPL(sampler, launchSize);

        vplCache[launchID.x] = vpl;
    } else {
        vplCache[launchID.x].age++;
    }
}

VPLReconnectionInfo generateVPL(inout Sampler sampler, uint2 launchSize) {
    uint x = sampler.UniformUIntFast(0, launchSize.x - 1);
    uint y = sampler.UniformUIntFast(0, launchSize.y - 1);

    uint2 launchID = uint2(x, y);

    IntersectionRecord its;
    getInitialSurfaceData(launchID, launchSize, its);
    
    RayDesc ray = generateVPLRay(its, sampler);

    VPLReconnectionInfo vpl;
    vpl.age = 0;
    VPLShading shadingInfo;

    createVPL(its, ray, vpl, shadingInfo);
    shadeVPL(vpl, shadingInfo, sampler, 4);
    setWeightedRadiance(vpl, shadingInfo, its);

    return vpl;
}