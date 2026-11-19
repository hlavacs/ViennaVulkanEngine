/** @file @brief Opt-in sRGB fallback for ImGui: decode authored colour/texture RGB before attachment encoding. */
[[vk::binding(0, 0)]] Sampler2D image; ///< ImGui's existing combined UNORM font/HUD texture.
struct GuiPush { float2 scale; float2 translate; }; ///< Layout matches the backend's four vertex-stage floats.
[[vk::push_constant]] ConstantBuffer<GuiPush> gui;
struct GuiVertex {
    float4 position : SV_Position; ///< Clip position from ImDrawVert pixel coordinates.
    [[vk::location(0)]] float4 color : COLOR0; ///< Interpolated authored sRGB widget colour.
    [[vk::location(1)]] float2 uv : TEXCOORD0; ///< ImGui texture coordinates.
};
/// @brief Match the ImGui Vulkan backend's vertex inputs and push-constant ABI.
[shader("vertex")]
GuiVertex guiOutputVertex([[vk::location(0)]] float2 position : POSITION,
    [[vk::location(1)]] float2 uv : TEXCOORD0, [[vk::location(2)]] float4 color : COLOR0) {
    GuiVertex result;
    result.position = float4(position * gui.scale + gui.translate, 0, 1);
    result.color = color; result.uv = uv;
    return result;
}
/// @brief Undo authored sRGB only for RGB; retain alpha and let the sRGB attachment perform the final encoding.
float decode(float value) { return value <= .04045 ? value / 12.92 : pow((value + .055) / 1.055, 2.4); }
[shader("fragment")]
float4 guiOutputFragment(GuiVertex vertex) : SV_Target {
    float4 color = vertex.color * image.Sample(vertex.uv);
    return float4(decode(color.r), decode(color.g), decode(color.b), color.a);
}
