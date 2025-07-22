import Common;

struct CoarseVertex {
    float3 positionW    : POSITION;
    float3 normalW      : NORMAL;
    float2 uv           : TEXCOORD;
    float3 tangentW     : TANGENT;
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
ConstantBuffer<UniformBufferObjectTexture> gParamsObject;

[[vk::binding(1, 1)]]
Sampler2D albedoSampler;

//----------------------------------------------------------------------------

[[vk::push_constant]]
PushConstantMaterial pushConstant;

//----------------------------------------------------------------------------

[shader("vertex")]
VertexStageOutput main(
    float3 positionL    : POSITION,
    float3 normalL      : NORMAL,
    float2 uv           : TEXCOORD,
    float3 tangentL     : TANGENT)
{
    VertexStageOutput output = {};
    float3 positionW        = mul(gParamsObject.model, float4(positionL, 1.0)).xyz;
    float3 positionV        = mul(gParamsFrame.camera.view, float4(positionW, 1.0)).xyz;

    output.coarseVertex.positionW   = positionW;
    output.coarseVertex.normalW     = normalize(mul(gParamsObject.modelInvTranspose, float4(normalL, 0.0)).xyz);
    output.coarseVertex.uv          = gParamsObject.uvscale * uv;
    output.coarseVertex.tangentW    = mul(gParamsObject.model, float4(tangentL, 1.0)).xyz;
    output.sv_position              = mul(gParamsFrame.camera.proj, float4(positionV, 1.0));

    return output;
}

//----------------------------------------------------------------------------

[shader("fragment")]
GBufferOutput main(CoarseVertex input: CoarseVertex)
{
    GBufferOutput output = {};

    float4 tex = albedoSampler.Sample(input.uv);

    output.normal     = float4(input.normalW, 0.0);
    output.albedo     = albedoSampler.Sample(input.uv);
    output.metalRough = float4(pushConstant.metallicRoughness, 0.0, 0.0);

    return output;
}
