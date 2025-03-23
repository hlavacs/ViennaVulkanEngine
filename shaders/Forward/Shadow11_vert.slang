import Common;

struct VS_OUTPUT {
    float4 position  : SV_Position;
};

// binding B,S means that the resource is bound to binding B, set S

// set 0 ... per frame

[[vk::binding(0,0)]]
ConstantBuffer<UniformBufferFrame> gParamsFrame;

//----------------------------------------------------------------------------

// set 1 ... per object

[[vk::binding(0,1)]]
ConstantBuffer<UniformBufferObjectTexture> gParamsObject;

//----------------------------------------------------------------------------

[shader("vertex")]
VS_OUTPUT vertexMain(float3 positionL: POSITION )
{
    VS_OUTPUT output;
    float4 positionW = mul(gParamsObject.model, float4(positionL, 1.0));
    output.position = mul(gParamsFrame.camera.proj, positionW);

    return output;
}



