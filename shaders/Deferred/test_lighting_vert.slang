// This shader is just here for testing purposes
struct VSOutput {
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
};

struct GlobalData { float4 dummy; };

[vk::binding(0, 0)] ConstantBuffer<GlobalData> gGlobal;

[shader("vertex")]
VSOutput main(uint vertexId : SV_VertexID)
{
    VSOutput output;

    float2 positions[4] = {
        float2(-1.0, -1.0),
        float2( 1.0, -1.0),
        float2(-1.0,  1.0),
        float2( 1.0,  1.0)
    };
    output.position = float4(positions[vertexId], 0.0, 1.0);
    output.uv = (positions[vertexId] + 1.0) * 0.5;
    return output;
}