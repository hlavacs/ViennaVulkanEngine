import Common;

struct CoarseVertex {
    float3 positionW : POSITION;
    float3 normalW : NORMAL;
    float4 fragColor : COLOR;
};

struct VertexStageOutput {
    CoarseVertex coarseVertex : CoarseVertex;
    float4 sv_position : SV_Position;
};

// binding B,S means that the resource is bound to binding B, set S

// set 0 ... per frame

[[vk::binding(0, 0)]]
ConstantBuffer<UniformBufferFrame> gParamsFrame;

[[vk::binding(1, 0)]]
StructuredBuffer<Light> gLights;

//----------------------------------------------------------------------------

// set 1 ... per object

[[vk::binding(0, 1)]]
ConstantBuffer<UniformBufferObjectColor> gParamsObject;

//----------------------------------------------------------------------------

[[vk::push_constant]]
LightOffset offset;

//----------------------------------------------------------------------------

[shader("vertex")]
VertexStageOutput vertexMain(
    float3 position: POSITION,
    float3 normal: NORMAL)
{
    float3 worldPosition = mul(gParamsObject.model, float4(position, 1.0)).xyz;
    float3 viewPosition = mul(gParamsFrame.camera.view, float4(worldPosition, 1.0)).xyz;

    VertexStageOutput output;
    output.coarseVertex.positionW = mul(gParamsObject.model, float4(normal, 1.0)).xyz;
    output.coarseVertex.normalW = mul(gParamsObject.modelInvTranspose, float4(normal, 1.0)).xyz;
    output.coarseVertex.fragColor = gParamsObject.color.diffuse;
    output.sv_position = mul(gParamsFrame.camera.proj, float4(viewPosition, 1.0));

    return output;
}

//----------------------------------------------------------------------------

struct GBufferOutput {
    float4 pos : SV_Target0;
    float4 norm : SV_Target1;
    float4 albedo : SV_Target2;
};

[shader("fragment")]
GBufferOutput fragmentMain(CoarseVertex input: CoarseVertex) : SV_Target
{
    GBufferOutput output;
    Camera camera = gParamsFrame.camera;

    //float4 tex = albedoSampler.Sample(input.uv);

    output.pos = float4(input.positionW, 1.0);
    output.norm = float4(input.normalW, 1.0);
    output.albedo = float4(input.fragColor);

    return output;
}