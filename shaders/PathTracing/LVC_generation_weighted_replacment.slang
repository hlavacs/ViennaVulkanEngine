import Bidrirectional_Raytracing;
import raytracing;
import Random;
import LightVertex;
import LVC_replacment;
import Lightsource;

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

    float avgImportance = importanceSum[0].load() / lightVertexCacheSize;

    if (sampler.UniformFloat() < pKeep || isFirstFrame == 1) {
        // 1. Evaluate the generation PDF for the OLD vertex's current state
        // (You must implement this function based on how generateLightVertex() samples space)
        float pGen = lightSourcePdf(lightBuffer, lightVertexOld.lightIndex);

        // 2. Track the continuous PDF evolution
        if (isFirstFrame == 1) {
            lightVertexOld.selectionPdf = pGen;
        } else {
            lightVertexOld.selectionPdf = (lightVertexOld.selectionPdf * pKeep) + (1.0f - avgKeep) * pGen;
        }

        lightVertexOld.importance = 0.0;
        lightVertexOld.age++;

        lightVertexCache[launchID.x] = lightVertexOld;
    } else {
        LightVertex lightVertexNew = generateLightVertex(sampler);
        // When freshly generated, its selection PDF is just its birth PDF
        lightVertexNew.selectionPdf = lightSourcePdf(lightBuffer, lightVertexOld.lightIndex);
        lightVertexCache[launchID.x] = lightVertexNew;
    }

    /*
    if (sampler.UniformFloat() < pKeep || isFirstFrame == 1) {
        lightVertexOld.selectionPdf = (lightVertexOld.importance / avgImportance);
        lightVertexOld.importance = 0.0;
        lightVertexOld.age++;
        // this gets gets super small for the used vertecies, thats the oposite of what should happen
        //I think I might need the average probability that a newly generated sample survives?????
        //lightVertexOld.selectionPdf *= (pKeep / avgKeep);

        lightVertexCache[launchID.x] = lightVertexOld;
        
    } else {
        LightVertex lightVertexNew = generateLightVertex(sampler);
        lightVertexCache[launchID.x] = lightVertexNew;       
    }
    */

    importanceSum[0] = 0.0;
    keepSum[0] = 0.0;
}

