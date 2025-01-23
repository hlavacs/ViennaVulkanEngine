struct CoarseVertex {
    float3 normal;
    float2 uv;
};

struct VertexStageOutput {
    CoarseVertex coarseVertex : CoarseVertex;
    float4       sv_position  : SV_Position;
};

struct UniformBufferObject {
    float4x4 model;
    float4x4 modelInvTranspose;
    float4   color;
};

struct UniformBufferCamera {
    float4x4 view;
    float4x4 proj;
};

// binding 0,1 means that the uniform buffer is bound to binding 0, set 0
[[vk::binding(0,0)]]
ParameterBlock<UniformBufferCamera> gParamsFrame;

// binding 0,0 means that the uniform buffer is bound to binding 0, set 1
[[vk::binding(0,1)]]
ParameterBlock<UniformBufferObject> gParamsObject;

//VertexStageOutput vertexMain(AssembledVertex assembledVertex)

[shader("vertex")]
VertexStageOutput vertexMain( 
	float3 position : POSITION,
    float3 normal   : NORMAL,
    float2 uv 		: UV)
{
    float3 worldPosition = mul(gParamsObject.model, float4(position, 1.0)).xyz;
    float3 viewPosition = mul(gParamsFrame.view, float4(worldPosition, 1.0)).xyz;   

    VertexStageOutput output;
    output.coarseVertex.uv = uv;
    output.coarseVertex.normal = mul(gParamsObject.modelInvTranspose, float4(normal, 1.0)).xyz;
    output.sv_position = mul(gParamsFrame.proj, float4(viewPosition, 1.0));

    return output;
}



