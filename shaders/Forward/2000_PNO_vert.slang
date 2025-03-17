import Common;

struct CoarseVertex {
    float3 positionW : POSITION;
    float3 normalW : NORMAL;
    float4 fragColor : COLOR;
};

struct VertexStageOutput {
    CoarseVertex coarseVertex : CoarseVertex;
    float4       sv_position  : SV_Position;
};

// binding B,S means that the resource is bound to binding B, set S

// set 0 ... per frame

[[vk::binding(0,0)]]
ConstantBuffer<UniformBufferFrame> gParamsFrame;

//----------------------------------------------------------------------------

// set 1 ... per object

[[vk::binding(0,1)]]
ConstantBuffer<UniformBufferObjectColor> gParamsObject;

//----------------------------------------------------------------------------

[shader("vertex")]
VertexStageOutput vertexMain( 
	float3 position : POSITION,
    float3 normal   : NORMAL)
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

