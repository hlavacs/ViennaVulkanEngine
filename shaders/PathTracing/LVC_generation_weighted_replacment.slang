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

//over time this gets way toooooo bright if the replacment rate is low
[shader("raygeneration")]
void rgenMain()
{
    uint2 launchID = DispatchRaysIndex().xy;
    uint2 launchSize = DispatchRaysDimensions().xy;

    uint linear = (uint)launchID.x + (uint)launchID.y * (uint)launchSize.x;
    uint pixelSeed = Hash(linear ^ (seed * 0x9E3779B9u));

    Sampler sampler = Sampler(pixelSeed);

    LightVertex lightVertexOld = lightVertexCache[launchID.x];

    float importanceSumF = importanceSum[0].load();


    float pKeep = getKeepProbability(lightVertexOld.importance, importanceSumF, lightVertexCacheSize);

    float avgKeep = keepSum[0].load() / lightVertexCacheSize;

    if (sampler.UniformFloat() < pKeep && isFirstFrame != 1) {

        float pGen = lightVertexPDF(lightBuffer, lightVertexOld);

        //if the replacment rate is too low this goes biased. 
        //but the reason might be numerical error because 0 replacment rate also goes very biased 

        //there are two posibilities 1. This is correect:
        //lightVertexOld.selectionPdf = (lightVertexOld.selectionPdf * pKeep) + (1.0f - avgKeep) * pGen;

        // 2 this is correct but with the caviat that avgKeep needs to be the avgKeep of the generationthis light vertex belongs to (aka avgKeep per age)
        lightVertexOld.selectionPdf = lightVertexOld.selectionPdf * (pKeep / avgKeep) ;
        //both work reasonably well as long as the noise in pKeep is not to high (don't replace the light vertices with guarantee when their importance is 0)

        //methode 2 is almost certainly correct, it preserves the shadows

        lightVertexOld.importance = 0.0;
        lightVertexOld.age++;

        lightVertexCache[launchID.x] = lightVertexOld;
    } else {

        LightVertex lightVertexNew = generateLightVertex(sampler);
        float pGen = lightVertexPDF(lightBuffer, lightVertexNew);

        if (importanceSumF <= 0.01) {
            lightVertexNew.selectionPdf = pGen;
        } else {
            lightVertexNew.selectionPdf = pGen;
            //lightVertexNew.selectionPdf = pGen * (1.0f - pKeep);
            // maybe the thing below would be correct if I would get any valid data on what pKeep is
            //lightVertexNew.selectionPdf = pGen * (1.0f - avgKeep) / (1.0f - pKeep);
            // lightVertexNew.selectionPdf = pGen * (1.0f - avgKeep) / (1.0f - pKeep);
        }

        lightVertexNew.importance = 0.0;
        lightVertexNew.age = 0;

        lightVertexCache[launchID.x] = lightVertexNew;
    }
}

// no improvment but it is interesting that it sort of
/*
[shader("raygeneration")]
void rgenMain()
{
    uint2 launchID = DispatchRaysIndex().xy;
    uint2 launchSize = DispatchRaysDimensions().xy;

    uint linear = (uint)launchID.x + (uint)launchID.y * (uint)launchSize.x;
    uint pixelSeed = Hash(linear ^ (seed * 0x9E3779B9u));

    Sampler sampler = Sampler(pixelSeed);

    LightVertex lightVertexOld = lightVertexCache[launchID.x];

    float importanceSumF = importanceSum[0].load();

    float rawKeep = getKeepProbability(lightVertexOld.importance, importanceSumF, lightVertexCacheSize);

    // CRITICAL FIX 1: Cap pKeep so the math never explodes.
    // This ensures every vertex is eventually replaced, maintaining cache churn.
    float pKeep = min(rawKeep, 1.0f);

    float avgKeep = keepSum[0].load() / lightVertexCacheSize;

    if (sampler.UniformFloat() < pKeep || isFirstFrame == 1) {
        float pGen = lightVertexPDF(lightBuffer, lightVertexOld);

        // Divide by avgKeep to turn this into a valid conditional PDF
        lightVertexOld.selectionPdf = (lightVertexOld.selectionPdf * pKeep) / avgKeep;

        lightVertexOld.importance = 0.0;
        lightVertexOld.age++;
        lightVertexCache[linear.x] = lightVertexOld;
    } else {
        LightVertex lightVertexNew = generateLightVertex(sampler);
        float pGen = lightVertexPDF(lightBuffer, lightVertexNew);

        // This remains correct because its conditional PDF given replacement is exactly pGen
        lightVertexNew.selectionPdf = pGen;

        lightVertexNew.importance = 0.0;
        lightVertexNew.age = 0;
        lightVertexCache[linear.x] = lightVertexNew;
    }
}
*/

// interesting but large scale noise!!!!!!
/*
[shader("raygeneration")]
void rgenMain()
{
    uint2 launchID = DispatchRaysIndex().xy;
    uint2 launchSize = DispatchRaysDimensions().xy;

    uint linear = (uint)launchID.x + (uint)launchID.y * (uint)launchSize.x;
    uint pixelSeed = Hash(linear ^ (seed * 0x9E3779B9u));

    Sampler sampler = Sampler(pixelSeed);

    LightVertex lightVertexOld = lightVertexCache[launchID.x];

    float importanceSumF = importanceSum[0].load();

    float rawKeep = getKeepProbability(lightVertexOld.importance, importanceSumF, lightVertexCacheSize);

    // CRITICAL FIX 1: Cap pKeep so the math never explodes.
    // This ensures every vertex is eventually replaced, maintaining cache churn.
    float pKeep = min(rawKeep, 0.95f);

    float avgKeep = keepSum[0].load() / lightVertexCacheSize;

    if (sampler.UniformFloat() < pKeep || isFirstFrame == 1) {

        float pGen = lightVertexPDF(lightBuffer, lightVertexOld);

        // CRITICAL FIX 2: Stable, non-recursive PDF.
        // Because pKeep is capped at 0.95, the denominator is always at least 0.05.
        // The PDF remains perfectly stable and stops the image from endlessly darkening.
        lightVertexOld.selectionPdf = pGen * (1.0f - avgKeep) / (1.0f - pKeep);

        lightVertexOld.importance = 0.0;
        lightVertexOld.age++;

        lightVertexCache[launchID.x] = lightVertexOld;
    } else {

        LightVertex lightVertexNew = generateLightVertex(sampler);
        float pGen = lightVertexPDF(lightBuffer, lightVertexNew);

        // CRITICAL FIX 3: Use the exact same formula.
        // By relying on the pixel's stable pKeep rather than dynamically calculating it,
        // new vertices blend seamlessly with old ones. Zero contrast flashes.
        lightVertexNew.selectionPdf = pGen * (1.0f - avgKeep) / (1.0f - pKeep);

        lightVertexNew.importance = 0.0;
        lightVertexNew.age = 0;

        lightVertexCache[launchID.x] = lightVertexNew;
    }
}
*/

// very wrong
/*
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

        float pGen = lightVertexPDF(lightBuffer, lightVertexOld);

        // FIX: Calculate the exact stationary density directly.
        // Clamp the denominator to prevent division by zero for immortal vertices.
        float stationaryPdf = pGen * (1.0f - avgKeep) / max(1.0f - pKeep, 0.0001f);

        lightVertexOld.selectionPdf = stationaryPdf;
        lightVertexOld.importance = 0.0;
        lightVertexOld.age++;

        lightVertexCache[launchID.x] = lightVertexOld;
    } else {
        LightVertex lightVertexNew = generateLightVertex(sampler);

        float pGenNew = lightVertexPDF(lightBuffer, lightVertexNew);

        // FIX: A fresh vertex has 0 importance, so calculate what its pKeep would be.
        float pKeepNew = getKeepProbability(0.0f, importanceSumF, lightVertexCacheSize);
        float stationaryPdfNew = pGenNew * (1.0f - avgKeep) / max(1.0f - pKeepNew, 0.0001f);

        lightVertexNew.selectionPdf = stationaryPdfNew;
        lightVertexNew.importance = 0.0;
        lightVertexNew.age = 0;

        lightVertexCache[launchID.x] = lightVertexNew;
    }
}
*/

// adapted code too dark but promising!
/*
[shader("raygeneration")]
void rgenMain()
{
    uint2 launchID = DispatchRaysIndex().xy;
    uint2 launchSize = DispatchRaysDimensions().xy;

    uint linear = (uint)launchID.x + (uint)launchID.y * (uint)launchSize.x;
    uint pixelSeed = Hash(linear ^ (seed * 0x9E3779B9u));

    Sampler sampler = Sampler(pixelSeed);

    LightVertex lightVertexOld = lightVertexCache[launchID.x];

    float importanceSumF = importanceSum[0].load();

    float pKeep = getKeepProbability(lightVertexOld.importance, importanceSumF, lightVertexCacheSize);
    float avgKeep = keepSum[0].load() / lightVertexCacheSize;

    if (sampler.UniformFloat() < pKeep || isFirstFrame == 1) {

        float pGen = lightVertexPDF(lightBuffer, lightVertexOld);

        if (isFirstFrame == 1) {
            lightVertexOld.selectionPdf = pGen;
        } else {
            // Reverting to your original tracking logic
            lightVertexOld.selectionPdf = (lightVertexOld.selectionPdf * pKeep) + (1.0f - avgKeep) * pGen;
        }

        lightVertexOld.importance = 0.0;
        lightVertexOld.age++;

        lightVertexCache[launchID.x] = lightVertexOld;
    } else {
        LightVertex lightVertexNew = generateLightVertex(sampler);

        // Use the proper light index for the NEW vertex
        float pGen = lightVertexPDF(lightBuffer, lightVertexNew);

        // THE FIX: A fresh vertex only represents a fraction of the cache density.
        // This completely solves the dark far-away regions.
        lightVertexNew.selectionPdf = pGen * (1.0f - avgKeep);

        lightVertexNew.importance = 0.0;
        lightVertexNew.age = 0;

        lightVertexCache[launchID.x] = lightVertexNew;
    }
}

*/

// was intendet to be the same but is not why?

/*
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

    // related concepts for research:
    // this is a Continuous-State Markov Chain
    // continuous version of the Chapman-Kolmogorov equation
    if (sampler.UniformFloat() < pKeep || isFirstFrame == 1) {
        // 1. Evaluate the generation PDF for the OLD vertex's current state

        //the pdf is almost certainly incorret and this is compounding the error. But then again it makes no sense why this only happens in the distance
        //also try to use atomics for the importance accumulation to see if that makes a diffrence
        float pGen = lightVertexPDF(lightBuffer, lightVertexOld);

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
        lightVertexNew.selectionPdf = lightVertexPDF(lightBuffer, lightVertexNew) * (1.0f - avgKeep);
        lightVertexCache[launchID.x] = lightVertexNew;
    }
}
*/

/*
//original:

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

    // related concepts for research:
    // this is a Continuous-State Markov Chain
    // continuous version of the Chapman-Kolmogorov equation
    if (sampler.UniformFloat() < pKeep || isFirstFrame == 1) {
        // 1. Evaluate the generation PDF for the OLD vertex's current state

        //the pdf is almost certainly incorret and this is compounding the error. But then again it makes no sense why this only happens in the distance
        //also try to use atomics for the importance accumulation to see if that makes a diffrence
        float pGen = lightVertexPDF(lightBuffer, lightVertexOld);

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
        lightVertexNew.selectionPdf = lightVertexPDF(lightBuffer, lightVertexNew);
        lightVertexCache[launchID.x] = lightVertexNew;
    }
}
*/