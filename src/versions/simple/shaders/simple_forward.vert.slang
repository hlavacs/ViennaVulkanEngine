/**
 * @file simple_forward.vert.slang
 * @brief Vertex stage for the simple forward renderer.
 *
 * Inputs: per-vertex position, color, and texture coordinate attributes.
 * Constants: shared view/projection matrices in set 0 binding 0, plus one per-object model matrix as push constants.
 * Outputs: clip-space position for Vulkan rasterization and interpolated vertex color for the fragment stage.
 */

struct FrameUniforms
{
    float4x4 view;       ///< Camera view matrix shared by all objects in the frame.
    float4x4 projection; ///< Camera projection matrix shared by all objects in the frame.
};

struct ObjectPushConstants
{
    float4x4 model; ///< Object-local model matrix selected before each draw call.
};

struct VertexInput
{
    [[vk::location(0)]] float3 position : POSITION; ///< Object-local vertex position.
    [[vk::location(1)]] float3 color    : COLOR0;   ///< Vertex color passed through to the fragment stage.
    [[vk::location(2)]] float2 texCoord : TEXCOORD0;///< Texture coordinate kept for tutorial-compatible vertex layouts.
};

struct VertexOutput
{
    float4 position : SV_Position;                  ///< Clip-space position consumed by Vulkan.
    [[vk::location(0)]] float3 color : COLOR0;      ///< Interpolated vertex color for simple forward output.
};

[[vk::binding(0, 0)]]
ConstantBuffer<FrameUniforms> frame; ///< Frame-wide camera constants.

[[vk::push_constant]]
ObjectPushConstants object; ///< Per-object transform constants.

[shader("vertex")]
VertexOutput vertexMain(VertexInput input)
{
    VertexOutput output;
    const float4 worldPosition = mul(object.model, float4(input.position, 1.0));    // Move from object to world space.
    const float4 viewPosition = mul(frame.view, worldPosition);                    // Move from world to camera space.
    output.position = mul(frame.projection, viewPosition);                         // Move from camera to clip space.
    output.color = input.color;                                                    // Preserve tutorial vertex color.
    return output;
}
