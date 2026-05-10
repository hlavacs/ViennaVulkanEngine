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

/// @brief Transforms the uploaded debug vertex with a real camera, but no lighting yet.
[shader("vertex")]
VveSceneVertexOutput vveSceneUnlitVertexMain(VveSceneVertexInput input)
{
    VveSceneVertexOutput output;
    output.clipPosition = vveSceneClipFromWorld(input.position);
    output.color = input.color;
    return output;
}

/// @brief Returns the uploaded flat debug color.
[shader("fragment")]
float4 vveSceneUnlitFragmentMain(VveSceneVertexOutput input) : SV_Target0
{
    return float4(input.color, 1.0);
}
