import Common;

struct CoarseVertex {
    float3 positionW    : POSITION;
    float3 normalW      : NORMAL;
    float4 fragColor    : COLOR;
};

struct VertexStageOutput {
    CoarseVertex coarseVertex   : CoarseVertex;
    float4 sv_position          : SV_Position;
};

// binding B,S means that the resource is bound to binding B, set S

// set 0 ... per frame

[[vk::binding(0, 0)]]
ConstantBuffer<UniformBufferFrame> gParamsFrame;

//----------------------------------------------------------------------------

// set 1 ... per object

[[vk::binding(0, 1)]]
ConstantBuffer<UniformBufferObjectColor> gParamsObject;

//----------------------------------------------------------------------------

[shader("vertex")]
VertexStageOutput main(
    float3 positionL : POSITION,
    float3 normalL   : NORMAL)
{
    float3 positionW    = mul(gParamsObject.model, float4(positionL, 1.0)).xyz;
    float3 positionV    = mul(gParamsFrame.camera.view, float4(positionW, 1.0)).xyz;

    VertexStageOutput output = {};
    output.coarseVertex.positionW   = positionW;
    output.coarseVertex.normalW     = normalize(mul(gParamsObject.modelInvTranspose, float4(normalL, 0.0)).xyz);
    output.coarseVertex.fragColor   = gParamsObject.color.diffuse;
    output.sv_position              = mul(gParamsFrame.camera.proj, float4(positionV, 1.0));

    return output;
}

//----------------------------------------------------------------------------

[shader("fragment")]
GBufferOutput main(CoarseVertex input: CoarseVertex)
{
    GBufferOutput output = {};

    output.pos      = float4(input.positionW, 1.0);
    output.norm     = float4(input.normalW * 0.5 + 0.5, 0.0);
    output.albedo   = input.fragColor;

    return output;
}