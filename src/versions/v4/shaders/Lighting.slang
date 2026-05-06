/// @file Shared lighting and debug math for the educational v4 forward renderer.

static const uint VVE_DEBUG_MODE_OFF         = 0; ///< No shader debug output is requested.
static const uint VVE_DEBUG_MODE_ATTACHMENTS = 1; ///< Extra color attachments contain intermediate values.
static const uint VVE_DEBUG_MODE_SAMPLES     = 2; ///< A storage buffer receives selected pixel samples.
static const uint VVE_DEBUG_MODE_FULL        = 3; ///< Attachments and storage-buffer samples are both active.
static const uint VVE_MAX_DEBUG_SAMPLES      = 64; ///< Maximum exact samples written by one debug frame.

/// @brief One directional light; direction points from the shaded point toward the light.
struct VveDirectionalLight
{
    float3 directionToLight; ///< Normalized world-space direction from surface point to light.
    float intensity;         ///< Linear intensity multiplier.
    float3 color;            ///< Linear RGB light color.
    float shadowStrength;    ///< 0 keeps shadows disabled, 1 applies full shadowing.
};

/// @brief Per-frame lighting parameters shared by forward and shadow-aware passes.
struct VveLightingConstants
{
    float3 ambientColor;        ///< Linear ambient term used to keep shadowed areas inspectable.
    float exposure;             ///< Final linear exposure multiplier.
    VveDirectionalLight sun;    ///< First renderer milestone uses one directional light.
    float4x4 lightView;         ///< Reserved for shadow verification and later shadow map pass.
    float4x4 lightProjection;   ///< Reserved for shadow verification and later shadow map pass.
    float shadowBias;           ///< Depth bias used by later shadow comparisons.
    uint enableShadows;         ///< Non-zero enables shadow-map sampling once the pass exists.
    uint debugMode;             ///< One of VVE_DEBUG_MODE_*.
    uint debugSampleCount;      ///< Number of requested exact debug samples.
};

/// @brief Surface data used by host-side mirror math and shader lighting.
struct VveSurfaceData
{
    float3 worldPosition; ///< World-space surface position.
    float3 worldNormal;   ///< Normalized world-space surface normal.
    float2 uv;            ///< Base material UV coordinate.
    float3 baseColor;     ///< Linear base color before lighting.
    float alpha;          ///< Surface alpha after material multiplication.
};

/// @brief Lighting result values intentionally named for readback verification.
struct VveLightingResult
{
    float3 ambient;        ///< Ambient contribution.
    float3 diffuse;        ///< Direct diffuse contribution before material multiplication.
    float nDotL;           ///< Lambert cosine term.
    float shadowFactor;    ///< 0 means fully shadowed, 1 means fully lit.
    float3 finalLighting;  ///< Sum of ambient plus direct light after shadowing.
};

/// @brief Returns a stable zero vector when a vector cannot be normalized.
float3 vveSafeNormalize(float3 value)
{
    float lengthSquared = dot(value, value);
    if (lengthSquared <= 1e-12) { return float3(0.0, 0.0, 0.0); }
    return value * rsqrt(lengthSquared);
}

/// @brief Encodes a signed unit vector into 0..1 RGB for debug color attachments.
float3 vveEncodeSignedUnit(float3 value)
{
    return (vveSafeNormalize(value) * 0.5) + 0.5;
}

/// @brief Computes clip-space light position for later CPU/GPU shadow verification.
float4 vveComputeLightClipPosition(VveLightingConstants lighting, float3 worldPosition)
{
    float4 lightViewPosition = mul(lighting.lightView, float4(worldPosition, 1.0));
    return mul(lighting.lightProjection, lightViewPosition);
}

/// @brief Converts a clip-space position to normalized shadow-map UV/depth coordinates.
float3 vveLightClipToShadowCoord(float4 lightClipPosition)
{
    float inverseW = lightClipPosition.w != 0.0 ? rcp(lightClipPosition.w) : 0.0;
    float3 ndc = lightClipPosition.xyz * inverseW;
    return float3((ndc.x * 0.5) + 0.5, (ndc.y * 0.5) + 0.5, ndc.z);
}

/// @brief Placeholder shadow factor until the shadow map pass and depth texture exist.
float vveEvaluateShadowFactor(VveLightingConstants lighting)
{
    return lighting.enableShadows != 0 ? 1.0 : 1.0;
}

/// @brief Evaluates the first deterministic directional-light model.
VveLightingResult vveEvaluateDirectionalLight(VveLightingConstants lighting, VveSurfaceData surface)
{
    float3 normal = vveSafeNormalize(surface.worldNormal);
    float3 directionToLight = vveSafeNormalize(lighting.sun.directionToLight);
    float nDotL = saturate(dot(normal, directionToLight));
    float rawShadow = vveEvaluateShadowFactor(lighting);
    float shadowFactor = lerp(1.0, rawShadow, saturate(lighting.sun.shadowStrength));

    VveLightingResult result;
    result.ambient = lighting.ambientColor;
    result.diffuse = lighting.sun.color * (lighting.sun.intensity * nDotL * shadowFactor);
    result.nDotL = nDotL;
    result.shadowFactor = shadowFactor;
    result.finalLighting = result.ambient + result.diffuse;
    return result;
}
