// This shader is just here for testing purposes
struct VSInput {
    float4 position : POSITION;
    float3 normal : NORMAL;
    float4 albedo : COLOR;
};

struct VSOutput {
    float4 position : SV_Position;
    float3 normal : TEXCOORD0;
    float4 albedo : TEXCOORD1;
};

struct GlobalData { float4 dummy; };

[vk::binding(0, 0)] ConstantBuffer<GlobalData> gGlobal;

[shader("vertex")]
VSOutput vertexMain(VSInput input) {
    VSOutput output;
    output.position = input.position;
    output.normal = input.normal;
    output.albedo = input.albedo;
    return output;
}
