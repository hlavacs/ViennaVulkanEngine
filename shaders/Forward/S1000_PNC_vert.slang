import Common;

struct CoarseVertex {
    float3 normal;
    float4 fragColor;
};

struct VertexStageOutput {
    CoarseVertex coarseVertex : CoarseVertex;
    float4       sv_position  : SV_Position;
};

// binding 0,1 means that the uniform buffer is bound to binding 0, set 0
[[vk::binding(0,0)]]
ParameterBlock<UniformBufferFrame> gParamsFrame;

// binding 0,0 means that the uniform buffer is bound to binding 0, set 1
[[vk::binding(0,1)]]
ParameterBlock<UniformBufferObject> gParamsObject;

[shader("vertex")]
VertexStageOutput vertexMain( 
	float3 position : POSITION,
    float3 normal   : NORMAL,
    float4 color 	: COLOR)
{
    float3 worldPosition = mul(gParamsObject.model, float4(position, 1.0)).xyz;
    float3 viewPosition = mul(gParamsFrame.camera.view, float4(worldPosition, 1.0)).xyz;

    VertexStageOutput output;
    output.coarseVertex.fragColor = color;
    output.coarseVertex.normal = mul(gParamsObject.modelInvTranspose, float4(normal, 1.0)).xyz;
    output.sv_position = mul(gParamsFrame.camera.proj, float4(viewPosition, 1.0));
 
    return output;
}

