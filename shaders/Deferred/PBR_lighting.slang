import Common;

struct VertexStageOutput {
    float4 sv_position : SV_Position;
    float2 uv        : TEXCOORD0;
};

[shader("vertex")]
VertexStageOutput main(uint vertexID : SV_VertexID)
{
    VertexStageOutput output = {};

    float2 pos[3] = {
        float2(-1, -1),
        float2(-1,  3),
        float2( 3, -1)
    };
    output.sv_position  = float4(pos[vertexID], 0.0, 1.0);
    output.uv           = (pos[vertexID] + 1.0) * 0.5;
    return output;
}

//----------------------------------------------------------------------------
// Set 0

[[vk::binding(0, 0)]]
ConstantBuffer<UniformBufferFrame> gParamsFrame;

[[vk::binding(1, 0)]]
StructuredBuffer<Light> gLights;

//----------------------------------------------------------------------------
// Set 1

[[vk::binding(0, 1)]]
Sampler2D gNormal;      // Normal from geometry shader

[[vk::binding(1, 1)]]
Sampler2D gAlbedo;      // Albedo from geometry shader

[[vk::binding(2, 1)]]
Sampler2D gMetallicRoughness; // Metallic and Roughness from geometry shader

[[vk::binding(3, 1)]]
Sampler2D<float> gDepth;       // Depth

//----------------------------------------------------------------------------
// Set 2

[[vk_binding(0, 2)]]
SamplerCubeArrayShadow shadowCubeArray;     // Pointlight cube shadowmaps

//----------------------------------------------------------------------------

[[vk::push_constant]]
PushConstantLight pushConstant;

//----------------------------------------------------------------------------

float3 reconstructPositionW(float2 uv, float depth) {
    float ndcX = uv.x * 2.0 - 1.0;
    float ndcY = uv.y * 2.0 - 1.0;
    float4 clipPos = float4(ndcX, ndcY, depth, 1.0);
    float4 positionW = mul(pushConstant.invViewProj, clipPos);
    positionW.xyz /= positionW.w;
    return positionW.xyz;
}

[shader("fragment")]
float4 main(VertexStageOutput in) : SV_Target
{
    float depth = gDepth.Sample(in.uv);
    if (depth >= 0.999) { // sky, nothing was drawn
        discard;
    }

    Camera camera = gParamsFrame.camera;
    uint3 numberLights = gParamsFrame.numberLights;

    float3 P = reconstructPositionW(in.uv, depth);
    float3 N = normalize(gNormal.Sample(in.uv).xyz);
    float3 A = gAlbedo.Sample(in.uv).xyz;
    float2 MR = gMetallicRoughness.Sample(in.uv).xy;

    float4 output = calculateLighting(gLights, numberLights, N, camera.positionW, P, A, MR.r, MR.g, shadowCubeArray);

    return output;
}
