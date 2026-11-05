#include "simple_forward.slang"
#include "simple_irradiance.slang" // Shared octahedral mapping; unused lighting bindings are stripped by Slang.
/** @file @brief GPU cubemap quadrature to L1 RGB SH, directional radial moments and inside-probe validity. */
[[vk::binding(5, 1)]] Texture2D<float4> probeCapture;
[[vk::binding(9, 1)]] Texture2D<float4> coarseProbeCapture;

/// @brief Matches lookAt's six world directions, stableUp and the Vulkan projection Y flip.
float3 probeDirection(uint face, float2 p)
{
    if (face == 0) return normalize(float3(1, -p.y, p.x));
    if (face == 1) return normalize(float3(-1, -p.y, -p.x));
    if (face == 2) return normalize(float3(p.x, 1, -p.y));
    if (face == 3) return normalize(float3(-p.x, -1, -p.y));
    if (face == 4) return normalize(float3(-p.x, -p.y, 1));
    return normalize(float3(p.x, -p.y, -1));
}
/// @brief Projects radiance (linear HDR radiance units) and convolves by the Lambert cosine kernel.
/// Orthonormal real SH: Y00=.282095, Y1x/y/z=.488603*d; convolution A0=pi, A1=2pi/3.
/// The stored coefficients represent E/pi: divide the cosine convolution by pi here, then lighting
/// multiplies by albedo*(1-metalness) exactly once. Neither exposure nor any previous indirect light enters capture.
[shader("fragment")]
float4 integrateMain(float4 position : SV_Position) : SV_Target0
{
    const uint coefficient = uint(position.x);
    const uint row = object.shadowMatrixIndex; // Capture-atlas slot is independent of the toroidal destination row.
    const float scale = VVE_PROBE_SCALE(object.flags); // Integration receives the same cascade as capture clipping.
    const bool moments = coefficient >= VVE_PROBE_SH_COEFFICIENTS;
    const uint texel = moments ? coefficient - VVE_PROBE_SH_COEFFICIENTS : 0;
    const float3 direction = probeOctDecode((float2(texel % VVE_PROBE_VISIBILITY_SIDE, texel / VVE_PROBE_VISIBILITY_SIDE) + .5) /
        float(VVE_PROBE_VISIBILITY_SIDE) * 2 - 1);
    float3 sum = float3(0);
    float totalWeight = 0;
    float momentWeight = 0;
    float2 hitFractions = float2(0);
    // Midpoint cubemap quadrature uses the solid-angle Jacobian, normalized to exactly 4pi.
    for (uint face = 0; face < VVE_POINT_SHADOW_FACES; ++face) {
        for (uint y = 0; y < VVE_PROBE_FACE_SIZE; ++y) {
            for (uint x = 0; x < VVE_PROBE_FACE_SIZE; ++x) {
                float2 p = (float2(x,y) + .5) / float(VVE_PROBE_FACE_SIZE) * 2.0 - 1.0;
                float3 d = probeDirection(face, p);
                float weight = pow(1.0 + dot(p,p), -1.5);
                const int3 pixel = int3(face * VVE_PROBE_FACE_SIZE + x, row * VVE_PROBE_FACE_SIZE + y, 0);
                const float4 capture = object.flags == 0 ? probeCapture.Load(pixel) : coarseProbeCapture.Load(pixel);
                const float distance = abs(capture.a); // Signed radial metres, independent of nonlinear/discarded D32.
                hitFractions += float2(capture.a < 0 ? 1 : 0, distance < 2 * VVE_PROBE_CAPTURE_NEAR * scale ? 1 : 0) * weight;
                if (moments) {
                    const float w = weight * pow(max(dot(d, direction), 0), VVE_PROBE_VISIBILITY_EXPONENT);
                    sum.xy += float2(distance, distance * distance) * w;
                    momentWeight += w;
                } else {
                    float basis = coefficient == 0 ? .2820947918 : .4886025119 * d[coefficient - 1];
                    sum += capture.rgb * basis * weight;
                }
                totalWeight += weight;
            }
        }
    }
    hitFractions /= totalWeight;
    if (moments) return float4(sum.xy / max(momentWeight, 1e-6), hitFractions);
    // Inactivation replaces relocation: closed interiors and near-surface degeneracies contribute no SH weight.
    const bool active = hitFractions.x <= VVE_PROBE_INACTIVE_BACKFACE && hitFractions.y <= VVE_PROBE_INACTIVE_NEAR;
    return float4(sum * (4.0 * kPi / totalWeight) * (coefficient == 0 ? 1.0 : 2.0 / 3.0), active ? 1.0 : 0.0);
}
