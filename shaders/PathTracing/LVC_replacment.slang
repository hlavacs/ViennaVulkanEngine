import Bidrirectional_Raytracing;



float getKeepProbability(float importance, float importanceSum) {

    float replacementRate = 0.1;
    float k = 5.0;

    float avgImportance = importanceSum / lightVertexCacheSize;
    float relativePerformance = importance / avgImportance;
    float pReplace = replacementRate / (1.0f + exp(k * (relativePerformance - 1.0f)));

    return 1.0 - pReplace;
}