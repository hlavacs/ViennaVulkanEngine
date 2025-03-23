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

//----------------------------------------------------------------------------

[[vk::binding(1, 0)]]
StructuredBuffer<ShadowIndex> gShadowIndex;

//----------------------------------------------------------------------------

[[vk::push_constant]]
ShadowImageArrayOffset offset;

//----------------------------------------------------------------------------

[maxvertexcount(3)]
[shader("geometry")]
void geometryMain(triangle VS_OUTPUT input[3], inout TriangleStream<GS_OUTPUT> outputStream, int instanceID: SV_InstanceID) {
    for (int j = 0; j < 3; j++) {
        GS_OUTPUT output;
        output.position = mul(gShadowIndex[offset.shadowIndexOffset + instanceID].lightSpaceMatrix, input[j].position);
        output.viewportIndex = gShadowIndex[offset.shadowIndexOffset + instanceID].viewportIndex;
        output.renderTargetIndex = j;
        outputStream.Append(output);
    }
    outputStream.RestartStrip();
}