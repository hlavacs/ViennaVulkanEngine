// This shader is just here for testing purposes
struct VSInput {
    float4 position : POSITION;
};

struct VSOutput {
    float4 position : SV_Position;
};

struct GlobalData { float4 dummy; };

[vk::binding(0, 0)] ConstantBuffer<GlobalData> gGlobal;

[shader("vertex")]
VSOutput vertexMain(VSInput input) {
    VSOutput output;
    output.position = input.position;
    return output;
}
