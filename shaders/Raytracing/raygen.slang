// Basic ray generation shader for VERendererRaytracing.
#include "common.slang"

[shader("raygeneration")]
void rgenMain()
{
    // Current pixel and full image size.
    const uint2 launchID = DispatchRaysIndex().xy;
    const float2 launchSize = (float2)DispatchRaysDimensions().xy;

    // Pixel center to normalized device coordinates in [-1, 1].
    const float2 pixelCenter = (float2)launchID + float2(0.5, 0.5);
    const float2 ndc = pixelCenter / launchSize * 2.0 - 1.0;

    // Build a primary ray from the inverse camera matrices.
    const float4 target = mul(camera.projInverse, float4(ndc, 1.0, 1.0));

    RayDesc ray;
    ray.Origin = mul(camera.viewInverse, float4(0.0, 0.0, 0.0, 1.0)).xyz;
    ray.Direction = mul(camera.viewInverse, float4(normalize(target.xyz), 0.0)).xyz;
    ray.TMin = 0.001;
    ray.TMax = 10000.0;

    HitPayload payload;
    payload.color = float3(0.0, 0.0, 0.0);
    payload.depth = 0;

    // AS, flags, instance mask, sbt offset, sbt stride, miss index, ray, payload.
    TraceRay(topLevelAS, RAY_FLAG_NONE, 0xff, 0, 0, 0, ray, payload);

    outImage[launchID] = float4(payload.color, 1.0);
}
