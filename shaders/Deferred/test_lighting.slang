import Common;

struct VertexStageOutput {
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
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
    output.position = float4(pos[vertexID], 0.0, 1.0);
    output.uv = (pos[vertexID] + 1.0) * 0.5;
    return output;
}

[[vk::binding(0, 0)]]
ConstantBuffer<UniformBufferFrame> gParamsFrame;

//----------------------------------------------------------------------------

[[vk::binding(0, 1)]]
Sampler2D gPosition;    // Position from geometry shader

[[vk::binding(1, 1)]]
Sampler2D gNormal;      // Normal from geometry shader

[[vk::binding(2, 1)]]
Sampler2D gAlbedo;      // Albedo from geometry shader

[[vk::binding(1, 0)]]
StructuredBuffer<Light> gLights;

//----------------------------------------------------------------------------

[[vk::push_constant]]
LightOffset offset;

//----------------------------------------------------------------------------

[shader("fragment")]
float4 main(VertexStageOutput in) : SV_Target
{
    Camera camera = gParamsFrame.camera;
    uint3 numberLights = gParamsFrame.numberLights;

    float3 P = gPosition.Sample(in.uv).xyz;
    // TODO: check formats, is * 2 - float3(1.0, 1.0, 1.0) needed?
    float3 N = (gNormal.Sample(in.uv).xyz) * 2 - float3(1.0, 1.0, 1.0);
    float3 A = gAlbedo.Sample(in.uv).xyz;

    float3 l = calculateLighting(gLights, numberLights, offset, N, camera.positionW, P);
    return float4(A.rgb * l, 1.0);
}