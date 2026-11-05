/** @file @brief Opt-in fullscreen HDR display transform; scene HDR is never modified.
 * Adapted from KhronosGroup/ToneMapping PBR_Neutral/pbrNeutral.glsl, Apache-2.0,
 * revision 180b1a7bddec33f73fe41712a2963cc3ad8e5547 (Copyright 2024 Khronos Group).
 * https://github.com/KhronosGroup/ToneMapping/blob/180b1a7bddec33f73fe41712a2963cc3ad8e5547/PBR_Neutral/pbrNeutral.glsl
 */
[[vk::binding(0, 0)]] Texture2D<float4> source; ///< Resolved HDR or the completed vvppl destination, in GENERAL.
struct OutputSettings {
    float exposure; ///< Validated positive finite multiplier, applied before Neutral.
    uint encodeSrgb; ///< UNORM output requires shader encoding; sRGB attachments encode themselves.
    uint neutral; ///< Zero selects the clamped identity used to prepare SMAA without tone mapping.
};
[[vk::push_constant]] ConstantBuffer<OutputSettings> settings;

/// @brief Vertex-buffer-free triangle covering every output pixel exactly once.
[shader("vertex")]
float4 toneMapVertex(uint id : SV_VulkanVertexID) : SV_Position {
    return float4(float2((id << 1) & 2, id & 2) * 2 - 1, 0, 1);
}

/// @brief Negative/NaN input becomes zero, +infinity becomes 65504 (largest FP16 scene value).
/// Exposed channels saturate at 1e6 before the reference operator, preventing finite-exposure overflow.
float exposed(float value) {
    value = isnan(value) || value < 0 ? 0 : min(value, 65504.0);
    return value > 0 && settings.exposure > 1e6 / value ? 1e6 : value * settings.exposure;
}

/// @brief Pinned Khronos PBR Neutral; ordinary nonnegative exposed values retain the reference behaviour.
float3 neutral(float3 color) {
    const float startCompression = 0.8 - 0.04;
    const float desaturation = 0.15;
    float x = min(color.r, min(color.g, color.b));
    float offset = x < 0.08 ? x - 6.25 * x * x : 0.04;
    color -= offset;
    float peak = max(color.r, max(color.g, color.b));
    if (peak < startCompression) return color;
    const float d = 1.0 - startCompression;
    float newPeak = 1.0 - d * d / (peak + d - startCompression);
    color *= newPeak / peak;
    float g = 1.0 - 1.0 / (desaturation * (peak - newPeak) + 1.0);
    return lerp(color, float3(newPeak), g);
}

/// @brief Encode RGB exactly once on UNORM SRGB_NONLINEAR surfaces; alpha is never transfer-encoded.
float srgb(float value) {
    return value <= 0.0031308 ? 12.92 * value : 1.055 * pow(value, 1.0 / 2.4) - 0.055;
}

/// @brief Output linear RGB through sRGB attachments, or encoded RGB through UNORM; preserve source alpha.
[shader("fragment")]
float4 toneMapFragment(float4 position : SV_Position) : SV_Target {
    float4 pixel = source.Load(int3(int2(position.xy), 0));
    float3 color = settings.neutral != 0 ? saturate(neutral(float3(exposed(pixel.r), exposed(pixel.g), exposed(pixel.b)))) : saturate(pixel.rgb);
    if (settings.encodeSrgb != 0) color = float3(srgb(color.r), srgb(color.g), srgb(color.b));
    return float4(color, pixel.a);
}
