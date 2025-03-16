import Common;

struct CoarseVertex {
    float3 positionW : POSITION;
    float3 normalW : NORMAL;
    float2 uv : TEXCOORD;
    float3 tangentW : TANGENT;
};

struct VertexStageOutput {
    CoarseVertex coarseVertex : CoarseVertex;
    float4       sv_position  : SV_Position;
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
VertexStageOutput vertexMain(
    float3 positionL: POSITION,
    float3 normalL: NORMAL,
    float2 uv: TEXCOORD,
    float3 tangentL : TANGENT)
{
    float3 positionW = mul(gParamsObject.model, float4(positionL, 1.0)).xyz;
    float3 positionV = mul(gParamsFrame.camera.view, float4(positionW, 1.0)).xyz;  

    VertexStageOutput output;
    output.coarseVertex.positionW   = positionW;
    output.coarseVertex.normalW     = mul(gParamsObject.modelInvTranspose, float4(normalL, 0.0)).xyz;
    output.coarseVertex.uv          = gParamsObject.uvscale * uv;
    output.coarseVertex.tangentW    = mul(gParamsObject.model, float4(tangentL, 1.0)).xyz;
    output.sv_position              = mul(gParamsFrame.camera.proj, float4(positionV, 1.0));

    return output;
}



