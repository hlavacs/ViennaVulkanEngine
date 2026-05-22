import Bidrirectional_Raytracing;
import raytracing;
import Random;
import LightVertex;
import LVC_replacment;

[[vk::binding(0, 2)]]
RWStructuredBuffer<Atomic<float>> importanceSum;

[[vk::binding(1, 2)]]
RWStructuredBuffer<Atomic<float>> keepSum;

[shader("raygeneration")]
void rgenMain()
{
    uint2 launchID = DispatchRaysIndex().xy;        // Current pixel (x,y)
    uint2 launchSize = DispatchRaysDimensions().xy; // Image size (width,height)

    uint linear = (uint)launchID.x + (uint)launchID.y * (uint)launchSize.x;
    uint pixelSeed = Hash(linear ^ (seed * 0x9E3779B9u));

    Sampler sampler = Sampler(pixelSeed);

    LightVertex lightVertexOld = lightVertexCache[launchID.x];

    float importanceSumF = importanceSum[0].load();

    float pKeep = getKeepProbability(lightVertexOld.importance, importanceSumF, lightVertexCacheSize); 
    float avgKeep = keepSum[0].load() / lightVertexCacheSize;

    if (sampler.UniformFloat() < pKeep || isFirstFrame == 1) {
        lightVertexOld.importance = 0.0;
        lightVertexOld.age++;
        lightVertexOld.selectionPdf *= (pKeep / avgKeep);

        lightVertexCache[launchID.x] = lightVertexOld;
        
    } else {
        LightVertex lightVertexNew = generateLightVertex(sampler);
        lightVertexCache[launchID.x] = lightVertexNew;       
    }
}

