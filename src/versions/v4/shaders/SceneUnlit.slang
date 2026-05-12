/// @file Minimal unlit scene shader for the first uploaded-geometry renderer milestone.

/// @brief CPU-provided vertex format: world-space position, normal, and linear RGB color.
struct VveSceneVertexInput
{
    float3 position : POSITION; ///< World-space position.
    float3 normal : NORMAL;     ///< World-space normal.
    float3 color : COLOR0;      ///< Flat debug color.
};

/// @brief Per-frame camera and one directional light supplied as push constants.
struct VveSceneFrame
{
    float4 clipFromWorld0; ///< First column of the world-to-clip matrix.
    float4 clipFromWorld1; ///< Second column of the world-to-clip matrix.
    float4 clipFromWorld2; ///< Third column of the world-to-clip matrix.
    float4 clipFromWorld3; ///< Fourth column of the world-to-clip matrix.
    float4 lightDirectionIntensity; ///< xyz direction to light, w intensity.
    float4 lightColor;              ///< xyz direct light color.
    float4 ambientColor;            ///< xyz ambient light color.
    float4 lightCenterRadius;        ///< xyz light-space center, w orthographic half size.
    float4 pointPositionRange;       ///< xyz point light position, w range.
    float4 pointColorIntensity;      ///< xyz point light color, w intensity.
    float4 spotPositionRange;        ///< xyz spot light position, w range.
    float4 spotDirectionCone;        ///< xyz spot direction, w outer cone angle.
    float4 spotColorIntensity;       ///< xyz spot light color, w intensity.
};

[[vk::push_constant]] ConstantBuffer<VveSceneFrame> gVveScene; ///< Tiny frame block for debug geometry.

/// @brief One exact GPU-side vertex sample copied back for CPU comparison.
struct VveSceneDebugSample
{
    uint vertexId;        ///< Source vertex id, or 0xffffffff when unused.
    float3 worldPosition; ///< Uploaded world position.
    float4 clipPosition;  ///< GPU-computed clip position.
    float4 lightClipPosition; ///< GPU-computed directional-light clip position.
    float4 spotLightClipPosition; ///< GPU-computed spot-light clip position.
    float3 ndcPosition;   ///< GPU-computed normalized device coordinate.
    float depth;          ///< GPU-computed Vulkan depth.
    float3 lightNdcPosition; ///< GPU-computed directional-light NDC position.
    float lightDepth;     ///< GPU-computed directional-light depth.
    float3 spotLightNdcPosition; ///< GPU-computed spot-light NDC position.
    float spotLightDepth; ///< GPU-computed spot-light depth.
    float sampledShadowDepth; ///< Depth sampled from the shadow map at this vertex.
    float shadowDepthDelta; ///< Light depth minus sampled shadow depth.
    float shadowBias;     ///< Bias used by the shadow comparison.
    float shadowFactor;   ///< One when lit, zero when shadowed.
    float sampledSpotShadowDepth; ///< Depth sampled from the spot shadow map.
    float spotShadowDepthDelta; ///< Spot depth minus sampled spot shadow depth.
    float spotShadowBias; ///< Bias used by the spot shadow comparison.
    float spotShadowFactor; ///< One when spot-lit, zero when spot-shadowed.
    float3 normal;        ///< GPU-normalized surface normal.
    float nDotL;          ///< Lambert cosine term.
    float3 directionToLight; ///< GPU-normalized direction to light.
    float intensity;      ///< Direct-light intensity.
    float3 ambientLighting; ///< Ambient lighting term.
    float directFactor;   ///< Scalar direct-light factor.
    float3 directLighting; ///< Direct lighting term.
    float unused0;        ///< Keeps CPU and shader layout explicit.
    float3 pointLighting; ///< Point-light term.
    float unused1;        ///< Keeps CPU and shader layout explicit.
    float3 spotLighting;  ///< Spot-light term.
    float unused2;        ///< Keeps CPU and shader layout explicit.
    float3 finalLighting; ///< Ambient plus direct lighting.
    float unused3;        ///< Keeps CPU and shader layout explicit.
};

RWStructuredBuffer<VveSceneDebugSample> gVveSceneDebug : register(u0, space0); ///< Debug readback samples.
Texture2D<float> gVveSceneShadowDepth : register(t1, space0); ///< Shadow depth sampled for proof data.
SamplerState gVveSceneShadowSampler : register(s2, space0); ///< Sampler used by the proof shadow read.
Texture2D<float> gVveSceneSpotShadowDepth : register(t3, space0); ///< Spot shadow depth sampled for proof data.

/// @brief Vertex-to-fragment payload.
struct VveSceneVertexOutput
{
    float4 clipPosition : SV_Position; ///< Clip-space position.
    float3 color : TEXCOORD0;          ///< Interpolated debug color.
    float3 normal : TEXCOORD1;         ///< Interpolated world-space normal.
    float4 lightClipPosition : TEXCOORD2; ///< Interpolated directional-light clip position.
    float3 worldPosition : TEXCOORD3;  ///< Interpolated world-space position.
};

/// @brief Applies the column-major camera matrix used by the CPU verification code.
float4 vveSceneClipFromWorld(float3 position)
{
    float4 p = float4(position, 1.0);
    return (gVveScene.clipFromWorld0 * p.x) +
           (gVveScene.clipFromWorld1 * p.y) +
           (gVveScene.clipFromWorld2 * p.z) +
           (gVveScene.clipFromWorld3 * p.w);
}

/// @brief Normalizes vectors defensively so CPU and GPU verification agree on degenerate data.
float3 vveSafeNormalize(float3 value)
{
    float len = length(value);
    return len > 1.0e-6 ? value / len : float3(0.0);
}

/// @brief Converts world space into a compact directional-light orthographic space.
float4 vveSceneLightClipFromWorld(float3 position)
{
    float3 directionToLight = vveSafeNormalize(gVveScene.lightDirectionIntensity.xyz);
    float3 seed = abs(directionToLight.y) > 0.95 ? float3(1.0, 0.0, 0.0) : float3(0.0, 1.0, 0.0);
    float3 right = vveSafeNormalize(cross(seed, directionToLight));
    float3 up = cross(directionToLight, right);
    float3 delta = position - gVveScene.lightCenterRadius.xyz;
    float radius = max(gVveScene.lightCenterRadius.w, 1.0e-4);
    return float4(dot(delta, right) / radius,
                  -dot(delta, up) / radius,
                  (radius - dot(delta, directionToLight)) / (2.0 * radius),
                  1.0);
}

/// @brief Converts world space into the perspective clip space of the spot light.
float4 vveSceneSpotLightClipFromWorld(float3 position)
{
    float3 spotDirection = vveSafeNormalize(gVveScene.spotDirectionCone.xyz);
    float3 seed = abs(spotDirection.y) > 0.95 ? float3(1.0, 0.0, 0.0) : float3(0.0, 1.0, 0.0);
    float3 right = vveSafeNormalize(cross(seed, spotDirection));
    float3 up = cross(spotDirection, right);
    float3 delta = position - gVveScene.spotPositionRange.xyz;
    float forwardDistance = dot(delta, spotDirection);
    float range = max(gVveScene.spotPositionRange.w, 0.10);
    float nearDistance = max(0.02, range * 0.01);
    float tanCone = max(0.01, tan(max(0.01, gVveScene.spotDirectionCone.w)));
    float w = max(forwardDistance, 1.0e-4);
    float z = ((forwardDistance - nearDistance) / max(1.0e-4, range - nearDistance)) * w;
    return float4(dot(delta, right) / tanCone, -dot(delta, up) / tanCone, z, w);
}

/// @brief Computes a hard-shadow comparison against the verified directional shadow map.
void vveSceneShadowTerms(float3 lightNdc, float nDotL, out float sampledDepth, out float bias, out float factor)
{
    float2 shadowUv = (lightNdc.xy * 0.5) + 0.5;
    bool insideShadow = shadowUv.x >= 0.0 && shadowUv.x <= 1.0 && shadowUv.y >= 0.0 && shadowUv.y <= 1.0 &&
                        lightNdc.z >= 0.0 && lightNdc.z <= 1.0;
    if (!insideShadow) {
        sampledDepth = 1.0;
        bias = 0.0;
        factor = 1.0;
        return;
    }

    uint shadowWidth;
    uint shadowHeight;
    gVveSceneShadowDepth.GetDimensions(shadowWidth, shadowHeight);
    float2 shadowMax = float2(float(shadowWidth - 1), float(shadowHeight - 1));
    uint2 shadowPixel = uint2(clamp(round(shadowUv * shadowMax), float2(0.0), shadowMax));
    float2 shadowTexelCenter = (float2(shadowPixel) + 0.5) / float2(float(shadowWidth), float(shadowHeight));
    sampledDepth = gVveSceneShadowDepth.SampleLevel(gVveSceneShadowSampler, shadowTexelCenter, 0.0);
    bias = max(0.0005, 0.003 * (1.0 - nDotL));
    factor = lightNdc.z - bias <= sampledDepth ? 1.0 : 0.0;
}

/// @brief Computes a hard-shadow comparison against the verified spot shadow map.
void vveSceneSpotShadowTerms(float3 lightNdc, float nDotL, out float sampledDepth, out float bias, out float factor)
{
    float2 shadowUv = (lightNdc.xy * 0.5) + 0.5;
    bool insideShadow = shadowUv.x >= 0.0 && shadowUv.x <= 1.0 && shadowUv.y >= 0.0 && shadowUv.y <= 1.0 &&
                        lightNdc.z >= 0.0 && lightNdc.z <= 1.0;
    if (!insideShadow) {
        sampledDepth = 1.0;
        bias = 0.0;
        factor = 1.0;
        return;
    }

    uint shadowWidth;
    uint shadowHeight;
    gVveSceneSpotShadowDepth.GetDimensions(shadowWidth, shadowHeight);
    float2 shadowMax = float2(float(shadowWidth - 1), float(shadowHeight - 1));
    uint2 shadowPixel = uint2(clamp(round(shadowUv * shadowMax), float2(0.0), shadowMax));
    float2 shadowTexelCenter = (float2(shadowPixel) + 0.5) / float2(float(shadowWidth), float(shadowHeight));
    sampledDepth = gVveSceneSpotShadowDepth.SampleLevel(gVveSceneShadowSampler, shadowTexelCenter, 0.0);
    bias = max(0.0005, 0.003 * (1.0 - nDotL));
    factor = lightNdc.z - bias <= sampledDepth ? 1.0 : 0.0;
}

/// @brief Computes one point-light contribution.
float3 vvePointLighting(float3 worldPosition, float3 normal)
{
    float3 offset = gVveScene.pointPositionRange.xyz - worldPosition;
    float distanceToLight = length(offset);
    if (distanceToLight <= 1.0e-6 || gVveScene.pointPositionRange.w <= 1.0e-6) { return float3(0.0); }
    float3 directionToLight = offset / distanceToLight;
    float nDotL = max(0.0, dot(normal, directionToLight));
    float attenuation = max(0.0, 1.0 - (distanceToLight / gVveScene.pointPositionRange.w));
    attenuation *= attenuation;
    return gVveScene.pointColorIntensity.xyz * (gVveScene.pointColorIntensity.w * nDotL * attenuation);
}

/// @brief Computes one spot-light contribution.
float3 vveSpotLighting(float3 worldPosition, float3 normal)
{
    float3 offset = gVveScene.spotPositionRange.xyz - worldPosition;
    float distanceToLight = length(offset);
    if (distanceToLight <= 1.0e-6 || gVveScene.spotPositionRange.w <= 1.0e-6) { return float3(0.0); }
    float3 directionToLight = offset / distanceToLight;
    float3 fromLight = -directionToLight;
    float cone = cos(max(0.01, gVveScene.spotDirectionCone.w));
    float spot = saturate((dot(fromLight, vveSafeNormalize(gVveScene.spotDirectionCone.xyz)) - cone) /
                          max(1.0e-6, 1.0 - cone));
    float nDotL = max(0.0, dot(normal, directionToLight));
    float attenuation = max(0.0, 1.0 - (distanceToLight / gVveScene.spotPositionRange.w));
    attenuation *= attenuation;
    return gVveScene.spotColorIntensity.xyz * (gVveScene.spotColorIntensity.w * nDotL * attenuation * spot);
}

/// @brief Stores deterministic vertex samples used by the host proof file.
void vveWriteSceneDebug(uint vertexId, float3 worldPosition, float3 normal, float4 clipPosition)
{
    uint sampleIndex = vertexId == 0 ? 0 : (vertexId == 6 ? 1 : 2);
    if (sampleIndex >= 2) { return; }

    float inverseW = abs(clipPosition.w) > 1.0e-6 ? rcp(clipPosition.w) : 0.0;
    float4 lightClipPosition = vveSceneLightClipFromWorld(worldPosition);
    float lightInverseW = abs(lightClipPosition.w) > 1.0e-6 ? rcp(lightClipPosition.w) : 0.0;
    float4 spotLightClipPosition = vveSceneSpotLightClipFromWorld(worldPosition);
    float spotLightInverseW = abs(spotLightClipPosition.w) > 1.0e-6 ? rcp(spotLightClipPosition.w) : 0.0;
    float3 unitNormal = vveSafeNormalize(normal);
    float3 directionToLight = vveSafeNormalize(gVveScene.lightDirectionIntensity.xyz);
    float3 directionToSpot = vveSafeNormalize(gVveScene.spotPositionRange.xyz - worldPosition);
    float nDotL = max(0.0, dot(unitNormal, directionToLight));
    float nDotSpot = max(0.0, dot(unitNormal, directionToSpot));
    float directFactor = gVveScene.lightDirectionIntensity.w * nDotL;
    VveSceneDebugSample sample;
    sample.vertexId = vertexId;
    sample.worldPosition = worldPosition;
    sample.clipPosition = clipPosition;
    sample.lightClipPosition = lightClipPosition;
    sample.spotLightClipPosition = spotLightClipPosition;
    sample.ndcPosition = clipPosition.xyz * inverseW;
    sample.depth = sample.ndcPosition.z;
    sample.lightNdcPosition = lightClipPosition.xyz * lightInverseW;
    sample.lightDepth = sample.lightNdcPosition.z;
    sample.spotLightNdcPosition = spotLightClipPosition.xyz * spotLightInverseW;
    sample.spotLightDepth = sample.spotLightNdcPosition.z;
    vveSceneShadowTerms(sample.lightNdcPosition, nDotL, sample.sampledShadowDepth,
                        sample.shadowBias, sample.shadowFactor);
    sample.shadowDepthDelta = sample.lightDepth - sample.sampledShadowDepth;
    vveSceneSpotShadowTerms(sample.spotLightNdcPosition, nDotSpot, sample.sampledSpotShadowDepth,
                            sample.spotShadowBias, sample.spotShadowFactor);
    sample.spotShadowDepthDelta = sample.spotLightDepth - sample.sampledSpotShadowDepth;
    sample.normal = unitNormal;
    sample.nDotL = nDotL;
    sample.directionToLight = directionToLight;
    sample.intensity = gVveScene.lightDirectionIntensity.w;
    sample.ambientLighting = gVveScene.ambientColor.xyz;
    sample.directFactor = directFactor;
    sample.directLighting = gVveScene.lightColor.xyz * directFactor;
    sample.unused0 = 0.0;
    sample.pointLighting = vvePointLighting(worldPosition, unitNormal);
    sample.unused1 = 0.0;
    sample.spotLighting = vveSpotLighting(worldPosition, unitNormal);
    sample.unused2 = 0.0;
    sample.finalLighting = sample.ambientLighting + sample.directLighting +
                           sample.pointLighting + sample.spotLighting;
    sample.unused3 = 0.0;
    gVveSceneDebug[sampleIndex] = sample;
}

/// @brief Transforms the uploaded debug vertex with a real camera, but no lighting yet.
[shader("vertex")]
VveSceneVertexOutput vveSceneUnlitVertexMain(VveSceneVertexInput input, uint vertexId : SV_VertexID)
{
    float4 clipPosition = vveSceneClipFromWorld(input.position);
    vveWriteSceneDebug(vertexId, input.position, input.normal, clipPosition);

    VveSceneVertexOutput output;
    output.clipPosition = clipPosition;
    output.color = input.color;
    output.normal = input.normal;
    output.lightClipPosition = vveSceneLightClipFromWorld(input.position);
    output.worldPosition = input.position;
    return output;
}

/// @brief Emits only light-space depth for the first verifiable shadow-map pass.
[shader("vertex")]
float4 vveSceneShadowVertexMain(VveSceneVertexInput input) : SV_Position
{
    return vveSceneLightClipFromWorld(input.position);
}

/// @brief Emits only spot-light-space depth for the first verifiable spot shadow-map pass.
[shader("vertex")]
float4 vveSceneSpotShadowVertexMain(VveSceneVertexInput input) : SV_Position
{
    return vveSceneSpotLightClipFromWorld(input.position);
}

/// @brief Applies the verified one-directional-light model to the uploaded debug color.
[shader("fragment")]
float4 vveSceneUnlitFragmentMain(VveSceneVertexOutput input) : SV_Target0
{
    float3 unitNormal = vveSafeNormalize(input.normal);
    float3 directionToLight = vveSafeNormalize(gVveScene.lightDirectionIntensity.xyz);
    float nDotL = max(0.0, dot(unitNormal, directionToLight));
    float lightInverseW = abs(input.lightClipPosition.w) > 1.0e-6 ? rcp(input.lightClipPosition.w) : 0.0;
    float3 lightNdc = input.lightClipPosition.xyz * lightInverseW;
    float sampledDepth;
    float shadowBias;
    float shadowFactor;
    vveSceneShadowTerms(lightNdc, nDotL, sampledDepth, shadowBias, shadowFactor);
    float3 direct = gVveScene.lightColor.xyz * (gVveScene.lightDirectionIntensity.w * nDotL * shadowFactor);
    float3 point = vvePointLighting(input.worldPosition, unitNormal);
    float4 spotLightClipPosition = vveSceneSpotLightClipFromWorld(input.worldPosition);
    float spotLightInverseW = abs(spotLightClipPosition.w) > 1.0e-6 ? rcp(spotLightClipPosition.w) : 0.0;
    float3 spotLightNdc = spotLightClipPosition.xyz * spotLightInverseW;
    float3 directionToSpot = vveSafeNormalize(gVveScene.spotPositionRange.xyz - input.worldPosition);
    float nDotSpot = max(0.0, dot(unitNormal, directionToSpot));
    float sampledSpotDepth;
    float spotShadowBias;
    float spotShadowFactor;
    vveSceneSpotShadowTerms(spotLightNdc, nDotSpot, sampledSpotDepth, spotShadowBias, spotShadowFactor);
    float3 spot = vveSpotLighting(input.worldPosition, unitNormal) * spotShadowFactor;
    return float4(input.color * (gVveScene.ambientColor.xyz + direct + point + spot), 1.0);
}
