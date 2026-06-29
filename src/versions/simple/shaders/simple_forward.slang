/**
 * @file simple_forward.slang
 * @brief Vertex and fragment stages for the simple forward renderer.
 *
 * Vertex inputs: per-vertex position, color, and texture coordinate attributes.
 * Vertex constants: shared view/projection matrices in set 0 binding 0, plus one per-object model matrix as push constants.
 * Shadow output: depth-only clip position from the light view-projection matrix.
 * Fragment inputs: interpolated vertex color and world-space position from the vertex stage.
 * Fragment resources: directional light shadow map in set 0 binding 1 for hard shadowing.
 * Fragment outputs: one opaque RGBA color attachment value for a shadowed forward render pass.
 */

struct FrameUniforms
{
    float4x4 view;       ///< Camera view matrix shared by all objects in the frame.
    float4x4 projection; ///< Camera projection matrix shared by all objects in the frame.
    float4x4 lightViewProj; ///< Light view-projection matrix for the upcoming shadow pass.
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
    float4 position : SV_Position;                       ///< Clip-space position consumed by Vulkan.
    [[vk::location(0)]] float3 color    : COLOR0;         ///< Interpolated vertex color for simple forward output.
    [[vk::location(1)]] float3 worldPos : TEXCOORD0;      ///< Interpolated world-space position for flat lighting.
};

struct FragmentInput
{
    [[vk::location(0)]] float3 color    : COLOR0;    ///< Interpolated vertex color.
    [[vk::location(1)]] float3 worldPos : TEXCOORD0; ///< Interpolated world-space position.
};

struct FragmentOutput
{
    [[vk::location(0)]] float4 color : SV_Target0; ///< Opaque color written to the framebuffer.
};

[[vk::binding(0, 0)]]
ConstantBuffer<FrameUniforms> frame; ///< Frame-wide camera constants.

[[vk::binding(1, 0)]]
Sampler2D shadowMap; ///< Directional light depth map sampled by the forward fragment stage.

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
    output.worldPos = worldPosition.xyz;
    return output;
}

[shader("vertex")]
float4 shadowVertexMain(VertexInput input) : SV_Position
{
    return mul(frame.lightViewProj, mul(object.model, float4(input.position, 1.0)));
}

[shader("fragment")]
FragmentOutput fragmentMain(FragmentInput input)
{
    FragmentOutput output;
    const float ambient = 0.2;                                           ///< Base light keeps back faces visible.
    const float3 lightDir = normalize(float3(-0.5, -1.0, 0.5));          ///< Directional light points downward toward the camera.
    const float3 normal = normalize(cross(ddx(input.worldPos), ddy(input.worldPos))); ///< Flat geometric face normal.
    const float diffuse = max(dot(normal, normalize(-lightDir)), 0.0);   ///< Lambertian light response.
    const float4 lightClip = mul(frame.lightViewProj, float4(input.worldPos, 1.0)); ///< Fragment position in light clip space.
    const float3 lightNdc = lightClip.xyz / lightClip.w;                ///< Vulkan light depth remains in 0..1 after divide.
    const float2 shadowUv = (lightNdc.xy * 0.5) + 0.5;                  ///< Convert light NDC XY to shadow texture UVs.
    const bool insideShadowMap = shadowUv.x >= 0.0 && shadowUv.x <= 1.0 &&
        shadowUv.y >= 0.0 && shadowUv.y <= 1.0 && lightNdc.z >= 0.0 && lightNdc.z <= 1.0; ///< Reject fragments outside light coverage.
    const float storedDepth = shadowMap.Sample(shadowUv).r;             ///< Closest surface depth from the light pass.
    const float shadowFactor = insideShadowMap && lightNdc.z - 0.001 > storedDepth ? 0.35 : 1.0; ///< Small 0.001 compare bias complements shadow-pass slope-scaled rasterizer bias.
    output.color = float4(input.color * (ambient + diffuse * shadowFactor), 1.0); ///< Apply ambient, diffuse, and hard shadowing to the vertex color.
    return output;
}
