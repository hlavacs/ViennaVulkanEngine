import Common;

struct VS_OUTPUT {
    float4 position : SV_Position;
};

struct GS_OUTPUT
{
    float4 position : SV_Position;
    uint viewportIndex : SV_ViewportArrayIndex;
    uint renderTargetIndex : SV_RenderTargetArrayIndex;
};

[[vk::binding(4, 0)]]
StructuredBuffer<ShadowIndex> gShadowIndex;

[[vk::push_constant]]
ShadowImageArrayOffset imageArrayOffset;


[maxvertexcount(6)]
[shader("geometry")]
void geometryMain(triangle VS_OUTPUT input[3], inout TriangleStream<GS_OUTPUT> outputStream, int instanceID: SV_InstanceID) {
    for (uint i = 0; i < MAX_ARRAY_SIZE; i++) {
        for (int j = 0; j < 3; j++) {
            GS_OUTPUT output;
            output.position = mul(gShadowIndex[instanceID].lightSpaceMatrix, input[j].position);
            output.viewportIndex = gShadowIndex[instanceID].viewportIndex;
            output.renderTargetIndex = i;
            outputStream.Append(output);
        }
        outputStream.RestartStrip();
    }
}