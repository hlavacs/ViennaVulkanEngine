// This shader is just here for testing purposes
import Common;

struct VSOutput {
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
};

[shader("vertex")]
VSOutput vertexMain(uint vertexID : SV_VertexID)
{
    VSOutput output = (VSOutput)0;

    float2 pos[3] = { { -1, -1 }, { 3, -1 }, { -1, 3 } };
    output.position = float4(pos[vertexID], 0.0, 1.0);
    output.uv = (pos[vertexID] + 1.0) * 0.5;
    return output;
}

struct PSOutput {
    float4 color : SV_Target;
};

[[vk::binding(0, 2)]]
Sampler2D gPosition; // Position from geometry shader

[[vk::binding(1, 2)]]
Sampler2D gNormal; // Normal from geometry shader

[[vk::binding(2, 2)]]
Sampler2D gAlbedo; // Albedo from geometry shader

[[vk::binding(1, 0)]]
StructuredBuffer<Light> gLights;

[shader("fragment")]
PSOutput fragmentMain(VSOutput in)
{
    // this is a dummy shader
    PSOutput out;

    float3 worldPos = gPosition.Sample(in.uv).xyz;
    float3 normMap = gNormal.Sample(in.uv).xyz;
    float3 albedo = gAlbedo.Sample(in.uv).xyz;

    float3 diffuse = 0.5;
    float3 ambient = 0.3;

    float3 color = diffuse + ambient;

    out.color = float4(1.0, 0.0, 1.0, 1.0);
    return out;
}