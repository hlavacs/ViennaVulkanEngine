struct VSInput {
    float4 position : POSITION;
};

struct VSOutput {
    float4 position : SV_Position;
};

[shader("vertex")]
VSOutput vertexMain(VSInput input) {
    VSOutput output;
    output.position = input.position;
    return output;
}
