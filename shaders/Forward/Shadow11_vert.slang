import Common;

struct CoarseVertex {
    float3 positionW : POSITION;
};

struct VertexStageOutput {
    float3 positionW : POSITION;
    float4 sv_position  : SV_Position;
};

// binding B,S means that the resource is bound to binding B, set S

// set 0 ... per frame

[[vk::binding(0,0)]]
ParameterBlock<UniformBufferFrame> gParamsFrame;

//----------------------------------------------------------------------------

// set 1 ... per object

[[vk::binding(0,1)]]
ParameterBlock<UniformBufferObjectTexture> gParamsObject;

//----------------------------------------------------------------------------

[[vk::push_constant]]
ShadowPushConstant shadowPushConstants;

//----------------------------------------------------------------------------

[shader("vertex")]
VertexStageOutput vertexMain(float3 positionL: POSITION )
{
    VertexStageOutput output;
    output.positionW = mul(gParamsObject.model, float4(positionL, 1.0)).xyz;
    output.sv_position = mul(gParamsFrame.camera.proj, float4(output.positionW, 1.0));

    return output;
}



