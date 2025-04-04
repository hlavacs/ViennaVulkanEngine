// This shader is just here for testing purposes
struct GBufferOutput {
    float4 pos    : SV_Target0;
    float4 norm   : SV_Target1;
    float4 albedo : SV_Target2;
};

struct GlobalData { float4 dummy; };

[vk::binding(0, 0)] ConstantBuffer<GlobalData> gGlobal;

[shader("fragment")]
GBufferOutput fragmentMain()
{
    GBufferOutput output;
    output.pos    = float4(1.0, 0.0, 0.0, 1.0); // Red: test for position
    output.norm   = float4(0.0, 1.0, 0.0, 1.0); // Green: test for normals
    output.albedo = float4(0.0, 0.0, 1.0, 1.0); // Blue: test for albedo
    return output;
}