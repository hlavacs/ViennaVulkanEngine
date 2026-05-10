/// @file Minimal unlit scene shader for the first uploaded-geometry renderer milestone.

/// @brief CPU-provided vertex format: clip-space x/y plus a linear RGB color.
struct VveSceneVertexInput
{
    float2 position : POSITION; ///< Clip-space position.
    float3 color : COLOR0;      ///< Flat debug color.
};

/// @brief Vertex-to-fragment payload.
struct VveSceneVertexOutput
{
    float4 clipPosition : SV_Position; ///< Clip-space position.
    float3 color : TEXCOORD0;          ///< Interpolated debug color.
};

/// @brief Forwards the uploaded debug vertex without camera or lighting.
[shader("vertex")]
VveSceneVertexOutput vveSceneUnlitVertexMain(VveSceneVertexInput input)
{
    VveSceneVertexOutput output;
    output.clipPosition = float4(input.position, 0.0, 1.0);
    output.color = input.color;
    return output;
}

/// @brief Returns the uploaded flat debug color.
[shader("fragment")]
float4 vveSceneUnlitFragmentMain(VveSceneVertexOutput input) : SV_Target0
{
    return float4(input.color, 1.0);
}
