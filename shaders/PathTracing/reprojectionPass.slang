import Reservoir;
import ReservoirGI;

[[vk::binding(0, 0)]]
cbuffer UniformBufferObject
{
    float4x4 view;
    float4x4 viewInv;
    float4x4 proj;
    float4x4 projInv;
    float4x4 prevView;
    float4x4 prevProj;
    uint seed;
    uint lightCount;
    uint x_dimensions;
    uint y_dimensions; 
};

[[vk::binding(1, 0)]]
RWTexture2D<float4> inPosition;

[[vk::binding(2, 0)]]
Sampler2D inPreviousPosition;

[[vk::binding(3, 0)]]
Sampler2D inPreviousLighting;

[[vk::binding(4, 0)]]
RWTexture2D<float4> outReprojectedLighting;

[[vk::binding(5, 0)]]
RWTexture2D<float4> outReprojectionError;

[[vk::binding(6, 0)]]
RWTexture2D<float4> outReprojectedPosition;

//restir

[[vk::binding(7, 0)]]
RWStructuredBuffer<Reservoir> inReservoir;

[[vk::binding(8, 0)]]
RWStructuredBuffer<Reservoir> outReservoir;

[[vk::binding(9, 0)]]
RWStructuredBuffer<ReservoirGI> inReservoirGI;

[[vk::binding(10, 0)]]
RWStructuredBuffer<ReservoirGI> outReservoirGI;



[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 threadId: SV_DispatchThreadID) {
    int2 launchID = threadId.xy;
    int2 launchSize = int2(x_dimensions, y_dimensions);

    float2 dims;
    inPreviousLighting.GetDimensions(dims.x, dims.y);
    float2 currentUV = (float2(launchID) + 0.5f) / dims;

    float4 position = float4(inPosition[launchID].xyz, 1.0);
    // float4 prevPosition = float4(inPreviousPosition.Load(int3(launchID,0)).xyz, 1.0);
    float4 prevPosition = float4(inPreviousPosition.SampleLevel(currentUV, 0).xyz, 1.0);

    // we ask where would the current position land in uv space viewed by the current camera (should just be the current uv).
    float4 clipSpacePos = mul(proj, mul(view, position));
    float3 ndc = clipSpacePos.xyz / clipSpacePos.w;
    float2 predictedUV = float3((ndc * 0.5) + 0.5).xy;

    //we ask where would the current position land in uv space if it was viewed by the previous camera.
    float4 clipSpacePosPrev = mul(prevProj, mul(prevView, position));
    float3 ndcPrev = clipSpacePosPrev.xyz / clipSpacePosPrev.w;
    float2 predictedUVPrev = float3((ndcPrev * 0.5) + 0.5).xy;

    // we ask where would the previous position land in uv space if it was viewed by the previous camera (should just be the current uv).
    float4 clipSpacePosLastFrame = mul(prevProj, mul(prevView, prevPosition));
    float3 ndcLastFrame = clipSpacePosLastFrame.xyz / clipSpacePosLastFrame.w;
    float2 predictedUVLastFrame = float3((ndcLastFrame * 0.5) + 0.5).xy;

    

    float2 uvMotion = predictedUVPrev - currentUV;

    float uvError = length(predictedUVLastFrame - currentUV);

    float3 lightingReprojected = inPreviousLighting.SampleLevel(predictedUVPrev, 0).xyz;
    float4 positionReprojected = float4(inPreviousPosition.SampleLevel(predictedUVPrev, 0).xyz,1.0);

    //restir
    float2 predictedUVPrevClamped = clamp(predictedUVPrev, 0.0, 1.0);
    
    //+0.5 because typconversion rounds down
    uint2 predictedIDPrev = (uint2)(predictedUVPrevClamped * (dims -1.0) + 0.5);

    if (predictedUVPrev.x >= 0.0 && predictedUVPrev.x <= 1.0 && predictedUVPrev.y >= 0.0 && predictedUVPrev.y <= 1.0) {
        storeReservoir(readReservoir(predictedIDPrev, launchSize, inReservoir), launchID, launchSize, outReservoir);
    } else {
        Reservoir reservoir = createEmptyReservoir();
        storeReservoir(reservoir, launchID, launchSize, outReservoir);
    }

    if (predictedUVPrev.x >= 0.0 && predictedUVPrev.x <= 1.0 && predictedUVPrev.y >= 0.0 && predictedUVPrev.y <= 1.0) {
        storeReservoirGI(readReservoirGI(predictedIDPrev, launchSize, inReservoirGI), launchID, launchSize, outReservoirGI);
    } else {
        ReservoirGI reservoir = createEmptyReservoirGI();
        storeReservoirGI(reservoir, launchID, launchSize, outReservoirGI);
    }
    

    float length = length(positionReprojected - position);

    bool isOffscreen = any(predictedUVPrev < 0.0) || any(predictedUVPrev > 1.0);
    if (isOffscreen) {
        length = 1000000.0;
    }

    outReprojectedLighting[launchID] = float4(lightingReprojected, 1.0);
    outReprojectionError[launchID] = float4(length);
    outReprojectedPosition[launchID] = positionReprojected;
}