

float getKeepProbability(float importance, float importanceSum, uint lightVertexCacheSize) {

    if (importance < 0.001) {
        return 0.0;
    }

    float replacementRate = 0.025;
    float k = 0.5;


    float avgImportance = importanceSum / lightVertexCacheSize;
    float relativePerformance = importance / avgImportance;
    float pReplace = replacementRate / (1.0f + exp(k * (relativePerformance - 1.0f)));

    return 1.0 - pReplace;
}