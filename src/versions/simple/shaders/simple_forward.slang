/**
 * @file simple_forward.slang
 * @brief Vertex and fragment stages for the simple forward renderer.
 *
 * Vertex inputs: per-vertex position, color, and texture coordinate attributes.
 * Vertex constants: shared view/projection matrices in set 0 binding 0, plus one per-object model matrix as push constants.
 * Fragment inputs: interpolated vertex color from the vertex stage.
 * Fragment outputs: one opaque RGBA color attachment value for a basic forward render pass.
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
    [[vk::location(0)]] float3 position : POSITION;  ///< Object-local vertex position.
    [[vk::location(1)]] float3 color    : COLOR0;    ///< Vertex color passed through to the fragment stage.
    [[vk::location(2)]] float2 texCoord : TEXCOORD0; ///< Texture coordinate kept for tutorial-compatible vertex layouts.
};

struct VertexOutput
{
    float4 position : SV_Position;             ///< Clip-space position consumed by Vulkan.
    [[vk::location(0)]] float3 color : COLOR0; ///< Interpolated vertex color for simple forward output.
};

struct FragmentInput
{
    [[vk::location(0)]] float3 color : COLOR0; ///< Interpolated vertex color.
};

struct FragmentOutput
{
    [[vk::location(0)]] float4 color : SV_Target0; ///< Opaque color written to the framebuffer.
};

[[vk::binding(0, 0)]]
ConstantBuffer<FrameUniforms> frame; ///< Frame-wide camera constants.

[[vk::push_constant]]
ObjectPushConstants object; ///< Per-object transform constants.

[shader("vertex")]
VertexOutput vertexMain(VertexInput input)
{
    VertexOutput output;
    const float4 worldPosition = mul(object.model, float4(input.position, 1.0));
    const float4 viewPosition = mul(frame.view, worldPosition);
    output.position = mul(frame.projection, viewPosition);
    output.color = input.color;
    return output;
}

[shader("fragment")]
FragmentOutput fragmentMain(FragmentInput input)
{
    FragmentOutput output;
    output.color = float4(input.color, 1.0);
    return output;
}
