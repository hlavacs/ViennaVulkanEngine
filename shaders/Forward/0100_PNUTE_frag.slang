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
ParameterBlock<UniformBufferFrame> gParamsFrame;

[[vk::binding(1, 0)]]
ConstantBuffer<Light[MAX_LIGHTS]> gLights;

[[vk::binding(2, 0)]]
Texture2DArray shadowMapArray[MAX_SHADOW_MAPS]; // shadow maps for the lights

[[vk::binding(3, 0)]]
SamplerState shadowSampler; // shadow map sampler

//----------------------------------------------------------------------------

// set 1 ... per object

[[vk::binding(0, 1)]]
ParameterBlock<UniformBufferObjectTexture> gParamsObject;

[[vk::binding(1,1)]]
Sampler2D texSampler;

//----------------------------------------------------------------------------

[shader("fragment")]
float4 fragmentMain(CoarseVertex cv : CoarseVertex) : SV_Target 
{
    Camera camera = gParamsFrame.camera;
    uint3 numberLights = gParamsFrame.numberLights;

    float4 tex = texSampler.Sample(cv.uv);
    float3 l = calculateLighting(gLights, numberLights, cv.normalW, camera.positionW, cv.positionW);
    return float4( tex.xyz*l, tex.w );
}







