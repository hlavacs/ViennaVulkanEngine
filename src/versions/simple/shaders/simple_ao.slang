#define VVE_AO 1
#include "simple_deferred.slang"
/** @file @brief Full-resolution deterministic horizon contact AO and a depth/normal bilateral filter. No history. */
[[vk::binding(7, 1)]] Texture2D<float> rawAo;

/// @brief Eight azimuths with four radial taps estimate local hemisphere visibility inside a 0.5 m sphere.
[shader("fragment")]
float aoMain(float4 position : SV_Position) : SV_Target0
{
    const int2 pixel = int2(position.xy);
    if (aoSourceDepth(pixel) >= 1) return 1;
    const float3 p = aoSourcePosition(pixel);
    const float3 n = normalize(mul((float3x3)frame.view, aoSourceNormal(pixel)));
    // A repeating 4x4 interleaved rotation is fixed in space and never depends on time or a random seed.
    const float angle = 6.28318530718 * frac(float((pixel.x & 3) + 4 * (pixel.y & 3)) * 0.61803398875);
    const float2 radiusPixels = VVE_AO_RADIUS * abs(float2(frame.projection[0][0], frame.projection[1][1])) * frame.viewport.xy / (2 * max(-p.z, 0.001));
    float occlusion = 0;
    // Maximum elevation per azimuth bounds cost and avoids counting the same blocker once per radial step.
    for (uint direction = 0; direction < VVE_AO_DIRECTIONS; ++direction) {
        const float a = angle + 6.28318530718 * float(direction) / VVE_AO_DIRECTIONS;
        const float2 ray = float2(cos(a), sin(a));
        float horizon = 0;
        for (uint step = 1; step <= VVE_AO_STEPS; ++step) {
            const int2 q = int2(floor(position.xy + ray * radiusPixels * float(step) / VVE_AO_STEPS));
            if (any(q < 0) || any(q >= int2(frame.viewport.xy)) || aoSourceDepth(q) >= 1) continue;
            const float3 delta = aoSourcePosition(q) - p;
            const float distance = length(delta);
            // Reject distant depth layers outright: a foreground silhouette cannot occlude a far wall.
            if (distance <= VVE_AO_BIAS || distance >= VVE_AO_RADIUS) continue;
            const float elevation = saturate((dot(n, delta) - VVE_AO_BIAS) / distance);
            horizon = max(horizon, elevation * (1 - distance * distance / (VVE_AO_RADIUS * VVE_AO_RADIUS)));
        }
        occlusion += horizon;
    }
    return saturate(1 - occlusion / VVE_AO_DIRECTIONS);
}

/// @brief A single 5x5 bilateral pass smooths interleaved noise without moving visibility across surface edges.
[shader("fragment")]
float aoFilterMain(float4 position : SV_Position) : SV_Target0
{
    const int2 pixel = int2(position.xy);
    if (aoSourceDepth(pixel) >= 1) return 1;
    const float3 p = aoSourcePosition(pixel), n = aoSourceNormal(pixel);
    float total = 0, weight = 0;
    // Reject sky, normal breaks and linear-depth gaps before weighting nearby pixels.
    for (int y = -2; y <= 2; ++y) for (int x = -2; x <= 2; ++x) {
        const int2 q = pixel + int2(x,y);
        if (any(q < 0) || any(q >= int2(frame.viewport.xy)) || aoSourceDepth(q) >= 1) continue;
        const float depthGap = abs(aoSourcePosition(q).z - p.z);
        const float similarity = dot(n, aoSourceNormal(q));
        if (depthGap >= VVE_AO_FILTER_DEPTH || similarity < VVE_AO_NORMAL_COS) continue;
        const float w = exp(-float(x*x + y*y) / 4.0) * (1 - depthGap / VVE_AO_FILTER_DEPTH) * pow(max(similarity,0), 8);
        total += w * rawAo.Load(int3(q,0)); weight += w;
    }
    return total / max(weight, 0.000001);
}
