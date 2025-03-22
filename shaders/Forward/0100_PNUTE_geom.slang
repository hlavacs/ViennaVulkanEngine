import Common;

struct CoarseVertex {
    float3 normal;
    float2 uv;
    float3 tangent;
};

struct VertexStageOutput {
    CoarseVertex coarseVertex : CoarseVertex;
    float4       sv_position  : SV_Position;
};

struct GSOutput {
    float4 position : SV_POSITION;
    uint renderTargetIndex : SV_RenderTargetArrayIndex;  // Route to shadow map
};

// binding 0,1 means that the uniform buffer is bound to binding 0, set 0
[[vk::binding(0, 0)]]
ParameterBlock<UniformBufferFrame> gParamsFrame;

[maxvertexcount(6)]
void main(triangle VertexStageOutput input[3], inout TriangleStream<GSOutput> stream) {
    for (uint i = 0; i < MAX_ARRAY_SIZE; i++) {  
        for (int j = 0; j < 3; j++) {
            GSOutput output;
            output.position = mul(lightSpaceMatrices[i], input[j].position);
            output.renderTargetIndex = i;
            stream.Append(output);
        }
        stream.RestartStrip();
    }
}