// This shader is just here for testing purposes
struct PSOutput {
    float4 color : SV_Target;
};

struct GlobalData { float4 dummy; };

[vk::binding(0, 0)] ConstantBuffer<GlobalData> gGlobal;

[shader("fragment")]
PSOutput main(float2 uv : TEXCOORD0)
{
    PSOutput output;
    output.color = float4(1.0, 1.0, 1.0, 1.0); // white for testing
    return output;
}