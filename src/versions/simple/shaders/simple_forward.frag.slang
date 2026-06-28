/**
 * @file simple_forward.frag.slang
 * @brief Fragment stage for the simple forward renderer.
 *
 * Inputs: interpolated vertex color from the vertex stage.
 * Outputs: one opaque RGBA color attachment value for a basic forward render pass.
 */

struct FragmentInput
{
    [[vk::location(0)]] float3 color : COLOR0; ///< Interpolated vertex color.
};

struct FragmentOutput
{
    [[vk::location(0)]] float4 color : SV_Target0; ///< Opaque color written to the framebuffer.
};

[shader("fragment")]
FragmentOutput fragmentMain(FragmentInput input)
{
    FragmentOutput output;
    output.color = float4(input.color, 1.0); // Use vertex color directly, matching the tutorial's simple pass.
    return output;
}
