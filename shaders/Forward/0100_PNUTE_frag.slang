import Common;

struct CoarseVertex {
    float3 positionW : POSITION;
    float3 normalW : NORMAL;
    float2 uv : TEXCOORD;
    float3 tangentW : TANGENT;
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
ConstantBuffer<UniformBufferObjectTexture> gParamsObject;

[[vk::binding(1,1)]]
Sampler2D texSampler;

//----------------------------------------------------------------------------

[[vk::push_constant]]
LightOffset offset;

//----------------------------------------------------------------------------

[shader("fragment")]
float4 fragmentMain(CoarseVertex cv : CoarseVertex) : SV_Target 
{
    Camera camera = gParamsFrame.camera;
    uint3 numberLights = gParamsFrame.numberLights;

    float4 tex = texSampler.Sample(cv.uv);
    float3 l = calculateLighting(gLights, numberLights, offset, cv.normalW, camera.positionW, cv.positionW);
    return float4( tex.rgb * l, 1 );
}







