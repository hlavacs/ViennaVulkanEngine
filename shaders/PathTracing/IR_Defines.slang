
//using this struct is highly inefficient for pdf estimation. Have a seperate buffer for the position!
struct VPLReconnectionInfo {
    float3 position;
    float3 normal;
    float3 weightedRadiance;
    uint age;
}

struct VPLShading {
    float3 radiance;
    float3 albedo;
}