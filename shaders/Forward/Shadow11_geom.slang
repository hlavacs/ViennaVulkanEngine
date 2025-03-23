import Common;

struct VS_OUTPUT {
    float4 svPosition : SV_POSITION;
};

struct GS_OUTPUT
{
    float4 svPosition : SV_POSITION;
    uint viewportIndex : SV_ViewportArrayIndex;
    uint renderTargetIndex : SV_RenderTargetArrayIndex;
};

// binding 0,1 means that the uniform buffer is bound to binding 0, set 0
[[vk::binding(0, 0)]]
StructuredBuffer<ShadowIndex> gShadowIndex;

[[vk::push_constant]]
cbuffer ShadowOffset {
    int arrayOffset;
};

[maxvertexcount(6)]
[shader("geometry")]
void geometryMain(triangle VS_OUTPUT input[3], inout TriangleStream<GS_OUTPUT> outputStream, int instanceID: SV_InstanceID) {
    for (uint i = 0; i < MAX_ARRAY_SIZE; i++) {
        for (int j = 0; j < 3; j++) {
            GS_OUTPUT output;
            //output.position = mul(lightSpaceMatrices[i], input[j].position);
            output.renderTargetIndex = i;
            outputStream.Append(output);
        }
        outputStream.RestartStrip();
    }
}