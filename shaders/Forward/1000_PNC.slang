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

[[vk::binding(2, 0)]]
Sampler2DArray<uint, 1> shadowMapImage; // shadow maps for the lights

//----------------------------------------------------------------------------

// set 1 ... per object

[[vk::binding(0, 1)]]
ConstantBuffer<UniformBufferObject> gParamsObject;

//----------------------------------------------------------------------------

[[vk::push_constant]]
LightOffset offset;

//----------------------------------------------------------------------------

[shader("vertex")]
VertexStageOutput vertexMain(
    float3 positionL: POSITION,
    float3 normalL: NORMAL,
    float4 color: COLOR)
{
    float3 PositionW = mul(gParamsObject.model, float4(positionL, 1.0)).xyz;
    float3 PositionV = mul(gParamsFrame.camera.view, float4(PositionW, 1.0)).xyz;

    VertexStageOutput output;
    output.coarseVertex.positionW = PositionW;
    output.coarseVertex.normalW = mul(gParamsObject.modelInvTranspose, float4(normalL, 1.0)).xyz;
    output.coarseVertex.fragColor = color;
    output.sv_position = mul(gParamsFrame.camera.proj, float4(PositionV, 1.0));

    return output;
}



[shader("fragment")]
float4 fragmentMain(CoarseVertex cv : CoarseVertex) : SV_Target
{
    Camera camera = gParamsFrame.camera;
    uint3 numberLights = gParamsFrame.numberLights;
    float3 l = calculateLighting(gLights, numberLights, offset, cv.normalW, camera.positionW, cv.positionW);
    return float4( l * cv.fragColor.rgb, 1 );
}


