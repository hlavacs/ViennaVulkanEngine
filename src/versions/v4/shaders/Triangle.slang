/// @file Minimal hardcoded triangle shader for verifying v4 pipeline execution.

/// @brief Vertex-to-fragment payload for the first pipeline smoke test.
struct VveTriangleVertexOutput
{
    float4 clipPosition : SV_Position; ///< Clip-space vertex position.
    float3 color : TEXCOORD0;          ///< Linear debug color.
};

/// @brief Emits one of three hardcoded vertices selected by SV_VertexID.
[shader("vertex")]
VveTriangleVertexOutput vveTriangleVertexMain(uint vertexId : SV_VertexID)
{
    float2 positions[3] = {float2(0.0, -0.55), float2(0.55, 0.45), float2(-0.55, 0.45)};
    float3 colors[3] = {float3(1.0, 0.2, 0.1), float3(0.1, 1.0, 0.2), float3(0.2, 0.4, 1.0)};

    VveTriangleVertexOutput output;
    output.clipPosition = float4(positions[vertexId], 0.0, 1.0);
    output.color = colors[vertexId];
    return output;
}

/// @brief Returns the interpolated debug color.
[shader("fragment")]
float4 vveTriangleFragmentMain(VveTriangleVertexOutput input) : SV_Target0
{
    return float4(input.color, 1.0);
}
