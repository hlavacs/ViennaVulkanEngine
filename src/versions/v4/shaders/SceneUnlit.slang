/// @file Minimal unlit scene shader for the first uploaded-geometry renderer milestone.

/// @brief CPU-provided vertex format: world-space position plus a linear RGB color.
struct VveSceneVertexInput
{
    float3 position : POSITION; ///< World-space position.
    float3 color : COLOR0;      ///< Flat debug color.
};

/// @brief Per-frame camera transform supplied as a push constant.
struct VveSceneCamera
{
    float4 clipFromWorld0; ///< First column of the world-to-clip matrix.
    float4 clipFromWorld1; ///< Second column of the world-to-clip matrix.
    float4 clipFromWorld2; ///< Third column of the world-to-clip matrix.
    float4 clipFromWorld3; ///< Fourth column of the world-to-clip matrix.
};

[[vk::push_constant]] ConstantBuffer<VveSceneCamera> gVveScene; ///< Tiny camera block for unlit geometry.

/// @brief One exact GPU-side vertex sample copied back for CPU comparison.
struct VveSceneDebugSample
{
    uint vertexId;        ///< Source vertex id, or 0xffffffff when unused.
    float3 worldPosition; ///< Uploaded world position.
    float4 clipPosition;  ///< GPU-computed clip position.
    float3 ndcPosition;   ///< GPU-computed normalized device coordinate.
    float depth;          ///< GPU-computed Vulkan depth.
};

RWStructuredBuffer<VveSceneDebugSample> gVveSceneDebug : register(u0, space0); ///< Debug readback samples.

/// @brief Vertex-to-fragment payload.
struct VveSceneVertexOutput
{
    float4 clipPosition : SV_Position; ///< Clip-space position.
    float3 color : TEXCOORD0;          ///< Interpolated debug color.
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

/// @brief Stores deterministic vertex samples used by the host proof file.
void vveWriteSceneDebug(uint vertexId, float3 worldPosition, float4 clipPosition)
{
    uint sampleIndex = vertexId == 0 ? 0 : (vertexId == 6 ? 1 : 2);
    if (sampleIndex >= 2) { return; }

    float inverseW = abs(clipPosition.w) > 1.0e-6 ? rcp(clipPosition.w) : 0.0;
    VveSceneDebugSample sample;
    sample.vertexId = vertexId;
    sample.worldPosition = worldPosition;
    sample.clipPosition = clipPosition;
    sample.ndcPosition = clipPosition.xyz * inverseW;
    sample.depth = sample.ndcPosition.z;
    gVveSceneDebug[sampleIndex] = sample;
}

/// @brief Transforms the uploaded debug vertex with a real camera, but no lighting yet.
[shader("vertex")]
VveSceneVertexOutput vveSceneUnlitVertexMain(VveSceneVertexInput input, uint vertexId : SV_VertexID)
{
    float4 clipPosition = vveSceneClipFromWorld(input.position);
    vveWriteSceneDebug(vertexId, input.position, clipPosition);

    VveSceneVertexOutput output;
    output.clipPosition = clipPosition;
    output.color = input.color;
    return output;
}

/// @brief Returns the uploaded flat debug color.
[shader("fragment")]
float4 vveSceneUnlitFragmentMain(VveSceneVertexOutput input) : SV_Target0
{
    return float4(input.color, 1.0);
}
