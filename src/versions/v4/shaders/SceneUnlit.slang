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
};

[[vk::push_constant]] ConstantBuffer<VveSceneFrame> gVveScene; ///< Tiny frame block for debug geometry.

/// @brief One exact GPU-side vertex sample copied back for CPU comparison.
struct VveSceneDebugSample
{
    uint vertexId;        ///< Source vertex id, or 0xffffffff when unused.
    float3 worldPosition; ///< Uploaded world position.
    float4 clipPosition;  ///< GPU-computed clip position.
    float4 lightClipPosition; ///< GPU-computed directional-light clip position.
    float3 ndcPosition;   ///< GPU-computed normalized device coordinate.
    float depth;          ///< GPU-computed Vulkan depth.
    float3 lightNdcPosition; ///< GPU-computed directional-light NDC position.
    float lightDepth;     ///< GPU-computed directional-light depth.
    float3 normal;        ///< GPU-normalized surface normal.
    float nDotL;          ///< Lambert cosine term.
    float3 directionToLight; ///< GPU-normalized direction to light.
    float intensity;      ///< Direct-light intensity.
    float3 ambientLighting; ///< Ambient lighting term.
    float directFactor;   ///< Scalar direct-light factor.
    float3 directLighting; ///< Direct lighting term.
    float unused0;        ///< Keeps CPU and shader layout explicit.
    float3 finalLighting; ///< Ambient plus direct lighting.
    float unused1;        ///< Keeps CPU and shader layout explicit.
};

RWStructuredBuffer<VveSceneDebugSample> gVveSceneDebug : register(u0, space0); ///< Debug readback samples.

/// @brief Vertex-to-fragment payload.
struct VveSceneVertexOutput
{
    float4 clipPosition : SV_Position; ///< Clip-space position.
    float3 color : TEXCOORD0;          ///< Interpolated debug color.
    float3 normal : TEXCOORD1;         ///< Interpolated world-space normal.
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

/// @brief Stores deterministic vertex samples used by the host proof file.
void vveWriteSceneDebug(uint vertexId, float3 worldPosition, float3 normal, float4 clipPosition)
{
    uint sampleIndex = vertexId == 0 ? 0 : (vertexId == 6 ? 1 : 2);
    if (sampleIndex >= 2) { return; }

    float inverseW = abs(clipPosition.w) > 1.0e-6 ? rcp(clipPosition.w) : 0.0;
    float4 lightClipPosition = vveSceneLightClipFromWorld(worldPosition);
    float lightInverseW = abs(lightClipPosition.w) > 1.0e-6 ? rcp(lightClipPosition.w) : 0.0;
    float3 unitNormal = vveSafeNormalize(normal);
    float3 directionToLight = vveSafeNormalize(gVveScene.lightDirectionIntensity.xyz);
    float nDotL = max(0.0, dot(unitNormal, directionToLight));
    float directFactor = gVveScene.lightDirectionIntensity.w * nDotL;
    VveSceneDebugSample sample;
    sample.vertexId = vertexId;
    sample.worldPosition = worldPosition;
    sample.clipPosition = clipPosition;
    sample.lightClipPosition = lightClipPosition;
    sample.ndcPosition = clipPosition.xyz * inverseW;
    sample.depth = sample.ndcPosition.z;
    sample.lightNdcPosition = lightClipPosition.xyz * lightInverseW;
    sample.lightDepth = sample.lightNdcPosition.z;
    sample.normal = unitNormal;
    sample.nDotL = nDotL;
    sample.directionToLight = directionToLight;
    sample.intensity = gVveScene.lightDirectionIntensity.w;
    sample.ambientLighting = gVveScene.ambientColor.xyz;
    sample.directFactor = directFactor;
    sample.directLighting = gVveScene.lightColor.xyz * directFactor;
    sample.unused0 = 0.0;
    sample.finalLighting = sample.ambientLighting + sample.directLighting;
    sample.unused1 = 0.0;
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
    return output;
}

/// @brief Emits only light-space depth for the first verifiable shadow-map pass.
[shader("vertex")]
float4 vveSceneShadowVertexMain(VveSceneVertexInput input) : SV_Position
{
    return vveSceneLightClipFromWorld(input.position);
}

/// @brief Applies the verified one-directional-light model to the uploaded debug color.
[shader("fragment")]
float4 vveSceneUnlitFragmentMain(VveSceneVertexOutput input) : SV_Target0
{
    float3 unitNormal = vveSafeNormalize(input.normal);
    float3 directionToLight = vveSafeNormalize(gVveScene.lightDirectionIntensity.xyz);
    float nDotL = max(0.0, dot(unitNormal, directionToLight));
    float3 direct = gVveScene.lightColor.xyz * (gVveScene.lightDirectionIntensity.w * nDotL);
    return float4(input.color * (gVveScene.ambientColor.xyz + direct), 1.0);
}
