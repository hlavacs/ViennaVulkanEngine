import Common;

struct CoarseVertex {
    float3 positionW : POSITION;
};

struct VertexStageOutput {
    float3 positionW : POSITION;
    float4 sv_position  : SV_Position;
};

// set 0 ... per frame

// binding 0,0 means that the uniform buffer is bound to binding 0, set 0
[[vk::binding(0,0)]]
ParameterBlock<UniformBufferFrame> gParamsFrame;

//----------------------------------------------------------------------------

// set 1 ... per object

// binding 0,1 means that the uniform buffer is bound to binding 0, set 1
[[vk::binding(0,1)]]
ParameterBlock<UniformBufferObjectTexture> gParamsObject;

//----------------------------------------------------------------------------

[shader("vertex")]
VertexStageOutput vertexMain(float3 positionL: POSITION )
{
    VertexStageOutput output;
    output.positionW = mul(gParamsObject.model, float4(positionL, 1.0)).xyz;
    output.sv_position = mul(gParamsFrame.camera.proj, float4(output.positionW, 1.0));

    return output;
}



