// This shader is just here for testing purposes
struct VSInput {
    float4 position : POSITION;
    float3 normal : NORMAL;
    float4 albedo : COLOR;
};

struct VSOutput {
    float4 position : SV_Position;
    float3 normal : TEXCOORD0;
    float4 albedo : TEXCOORD1;
};

struct GlobalData { float4 dummy; };

//[vk::binding(0, 0)] ConstantBuffer<GlobalData> gGlobal;

[vk::binding(1, 0)]
Sampler2DArray<uint, 1> positionImage;

[vk::binding(2, 0)]
Sampler2DArray<uint, 1> normalImage;

[vk::binding(3, 0)]
Sampler2DArray<uint, 1> albedoImage;

//[[vk::binding(4, 0)]]
//StructuredBuffer<Light> gLights;

[shader("vertex")]
VSOutput vertexMain(VSInput input) {
    VSOutput output;
    output.position = input.position;
    output.normal = input.normal;
    output.albedo = input.albedo;
    return output;
}

struct GBufferOutput {
    float4 pos : SV_Target0;
    float4 norm : SV_Target1;
    float4 albedo : SV_Target2;
};

[shader("fragment")]
GBufferOutput fragmentMain()
{
    GBufferOutput output;
    output.pos = float4(1.0, 0.0, 0.0, 1.0);    // Red: test for position
    output.norm = float4(0.0, 1.0, 0.0, 1.0);   // Green: test for normals
    output.albedo = float4(0.0, 0.0, 1.0, 1.0); // Blue: test for albedo
    return output;
}