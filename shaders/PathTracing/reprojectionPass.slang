[[vk::binding(0, 0)]]
cbuffer UniformBufferObject
{
    float4x4 view;
    float4x4 viewInv;
    float4x4 proj;
    float4x4 projInv;
    float4x4 prevView;
    float4x4 prevProj;
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

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 threadId: SV_DispatchThreadID) {
    int2 launchID = threadId.xy;

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

    float length = length(positionReprojected - position);

    outReprojectedLighting[launchID] = float4(lightingReprojected, 1.0);
    outReprojectionError[launchID] = float4(length);
}

/*
[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 threadId: SV_DispatchThreadID) {
    int2 launchID = threadId.xy;

    float2 dims;
    inPreviousLighting.GetDimensions(dims.x, dims.y);

    float4 position = float4(inPosition[launchID].xyz, 1.0);
    float4 prevPosition = float4(inPreviousPosition.Load(int3(launchID,0)).xyz, 1.0);

    float4 clipSpacePos = mul(proj, mul(view, position));
    float4 clipSpacePosPrev = mul(prevProj, mul(prevView, position));

    //perspective devide
    float3 ndc = clipSpacePos.xyz / clipSpacePos.w;
    float3 ndcPrev = clipSpacePosPrev.xyz / clipSpacePosPrev.w;

    // Calculate Motion in normalized device coordinates
    float2 motionNDC = ndcPrev.xy - ndc.xy;

    // Convert NDC motion to UV motion
    // Divide by 2 because ndc has the range(-1 to 1)
    // Flip Y because Vulkan UVs start at top-left
    float2 motionUV = float2(motionNDC.x, motionNDC.y);



    //Calculate the Reprojected UV
    float2 currentUV = (float2(launchID) + 0.5f) / dims;
    float2 reprojectedUV = currentUV + motionUV;

    float3 lightingReprojected = inPreviousLighting.SampleLevel(reprojectedUV, 0).xyz;
    float4 positionReprojected = float4(inPreviousPosition.SampleLevel(reprojectedUV, 0).xyz,1.0);



    float length = length(positionReprojected - position);

    outReprojectedLighting[launchID] = float4(length, length, length, 1.0);
}
*/

/*
[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 threadId: SV_DispatchThreadID) {
    int2 launchID = threadId.xy;

    float2 dims;
    inPreviousLighting.GetDimensions(dims.x, dims.y);
    float2 currentUV = (float2(launchID) + 0.5f) / dims;

    float4 position = float4(inPosition[launchID].xyz, 1.0);
    float4 prevPosition = float4(inPreviousPosition.Load(int3(launchID,0)).xyz, 1.0);

    float4 clipSpacePos = mul(proj, mul(view, position));
    float4 clipSpacePosPrev = mul(proj, mul(view, prevPosition));

    //perspective devide
    float3 ndc = clipSpacePos.xyz / clipSpacePos.w;
    float3 ndcPrev = clipSpacePosPrev.xyz / clipSpacePosPrev.w;

    float3 predictedUV = (ndc * 0.5) + 0.5;
    float3 predictedUVPrev = (ndcPrev * 0.5) + 0.5;

    float2 uvMotion = predictedUVPrev.xy - currentUV;

    float uvError = length(predictedUVPrev.xy - currentUV);

    // Calculate Motion in normalized device coordinates
    float2 motionNDC = ndcPrev.xy - ndc.xy;

    // Convert NDC motion to UV motion
    // Divide by 2 because ndc has the range(-1 to 1)
    // Flip Y because Vulkan UVs start at top-left
    float2 motionUV = float2(motionNDC.x, motionNDC.y);



    //Calculate the Reprojected UV

    float2 reprojectedUV = currentUV + motionUV;

    float3 lightingReprojected = inPreviousLighting.SampleLevel(reprojectedUV, 0).xyz;
    float4 positionReprojected = float4(inPreviousPosition.SampleLevel(predictedUVPrev.xy, 0).xyz,1.0);

    

    float length = length(positionReprojected - position);

    outReprojectedLighting[launchID] = float4(length, length, length, 1.0);
}
*/