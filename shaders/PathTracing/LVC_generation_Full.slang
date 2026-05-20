import Bidrirectional_Raytracing;
import raytracing;
import Random;
import LightVertex;

[shader("raygeneration")]
void rgenMain()
{
    uint2 launchID = DispatchRaysIndex().xy;        // Current pixel (x,y)
    uint2 launchSize = DispatchRaysDimensions().xy; // Image size (width,height)

    uint linear = (uint)launchID.x + (uint)launchID.y * (uint)launchSize.x;
    uint pixelSeed = Hash(linear ^ (seed * 0x9E3779B9u));

    Sampler sampler = Sampler(pixelSeed);

    LightVertex lightVertex = generateLightVertex(sampler);

    lightVertexCache[launchID.x] = lightVertex;
}