/** @file simple_material.slang
 * @brief GGX/Smith/Schlick direct lighting and deterministic, approximate panorama illumination.
 * Panorama filtering uses eight lobe samples plus ordinary mipmaps, avoiding new GPU resources.
 * It approximates a prefiltered environment and omits the split-sum BRDF integration and local reflections.
 */

/// @brief Schlick reflectance approaches unity at grazing angles.
float3 fresnelSchlick(float cosine, float3 f0)
{
    return f0 + (1.0 - f0) * pow(1.0 - cosine, 5.0);
}

/// @brief GGX distribution with separable Smith visibility; returns BRDF times the incident cosine.
float3 microfacetLight(float3 color, float metalness, float roughness, float3 n, float3 v, float3 l)
{
    const float nv = max(dot(n, v), 0.0001), nl = saturate(dot(n, l));
    const float3 h = normalizeOrZero(v + l); // Opposite directions have no specular response.
    const float nh = saturate(dot(n, h)), vh = saturate(dot(v, h));
    const float a = max(roughness * roughness, 0.0025), a2 = a * a;
    const float denominator = nh * nh * (a2 - 1.0) + 1.0;
    const float distribution = a2 / max(kPi * denominator * denominator, 1.0e-12);
    const float visibilityV = 2.0 * nv / (nv + sqrt(a2 + (1.0 - a2) * nv * nv));
    const float visibilityL = 2.0 * nl / max(nl + sqrt(a2 + (1.0 - a2) * nl * nl), 0.0001);
    const float3 fresnel = fresnelSchlick(vh, lerp(float3((object.flags & kWaterFlag) != 0 ? 0.02 : 0.04), color, metalness));
    return ((1.0 - fresnel) * color * (1.0 - metalness) / kPi +
        distribution * visibilityV * visibilityL * fresnel / max(4.0 * nv * nl, 0.0001)) * nl;
}

/// @brief Filters a panorama over a GGX reflection lobe; roughness one also approximates diffuse sky illumination.
float3 sampleEnvironment(float3 direction, float roughness)
{
    if (frame.environmentLighting.x < 0.0) return float3(0.0);
    const uint texture = uint(frame.environmentLighting.x);
    if (texture >= kMaxSceneTextures) return float3(0.0);
    uint width, height, levels;
    baseColorTextures[texture].GetDimensions(0, width, height, levels);
    const float3 n = normalize(direction);
    const float3 tangent = normalize(cross(abs(n.y) < 0.99 ? float3(0, 1, 0) : float3(1, 0, 0), n));
    const float3 bitangent = cross(n, tangent);
    const float a = max(roughness * roughness, 0.001);
    const float lod = min(float(levels - 1), roughness * float(levels - 1) * 0.65);
    const float edge = min(0.5, 0.5 * exp2(lod) / float(height)); // Coarse mip samples must not wrap over a pole.
    float3 result = float3(0.0);
    float weight = 0.0;
    // Fixed samples keep captures repeatable; mip filtering suppresses sparkle between the sparse samples.
    for (uint i = 0; i < 8; ++i) {
        const float u = (float(i) + 0.5) / 8.0, phi = float(i) * 2.39996323;
        const float cosine = sqrt((1.0 - u) / (1.0 + (a * a - 1.0) * u));
        const float sine = sqrt(max(0.0, 1.0 - cosine * cosine));
        const float3 h = tangent * (cos(phi) * sine) + bitangent * (sin(phi) * sine) + n * cosine;
        const float3 l = reflect(-n, h);
        const float w = saturate(dot(n, l));
        const float2 uv = float2(atan2(l.x, -l.z) / (2.0 * kPi) + 0.5,
            clamp(1.0 - acos(clamp(l.y, -1.0, 1.0)) / kPi, edge, 1.0 - edge));
        result += baseColorTextures[texture].SampleLevel(uv, lod).rgb * w;
        weight += w;
    }
    return result * frame.environmentLighting.y / max(weight, 0.0001);
}
