struct LightVertex {
    float4 radiance;
    float4 samplePosition;
    float4 hitPosition;
    float4 normal;
    float4 tangent;
    float2 uv;
    uint lightIndex;
    uint materialIndex;
    uint age;
    uint lastReevaluation;
    float selectionPdf;
    float importance;
}
