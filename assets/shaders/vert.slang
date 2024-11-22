
struct AssembledVertex
{
    float3 position : POSITION;
    float3 color    : COLOR;
    float2 uv       : UV;
};


struct CoarseVertex
{
    float3 fragColor;
    float2 uv;
};


struct VertexStageOutput
{
    CoarseVertex coarseVertex : CoarseVertex;
    float4       sv_position  : SV_Position;
};


struct UniformBufferObject {
    float4x4 model;
    float4x4 view;
    float4x4 proj;
};

ParameterBlock<UniformBufferObject> gParams;


[shader("vertex")]
VertexStageOutput vertexMain(AssembledVertex assembledVertex)
{
    VertexStageOutput output;

    float3 position = assembledVertex.position;
    float3 color    = assembledVertex.color;
    float2 uv       = assembledVertex.uv;

    float3 worldPosition = mul(gParams.model, float4(position, 1.0)).xyz;
    float3 viewPosition = mul(gParams.view, float4(worldPosition, 1.0)).xyz;   

    output.coarseVertex.fragColor   = color;
    output.coarseVertex.uv          = uv;
    output.sv_position = mul(gParams.proj, float4(viewPosition, 1.0));

    return output;
}





