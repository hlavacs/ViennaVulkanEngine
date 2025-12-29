[[vk::binding(0, 0)]]
cbuffer UniformBufferObject
{
    float4x4 view;
    float4x4 viewInv;
    float4x4 proj;
    float4x4 projInv;
};

struct VSInput
{
    float4 position  : POSITION0;
    float4 normal    : NORMAL0;
    float4 texCoord  : TEXCOORD0;

    float4x4 model;
    float4x4 modelInverse;
    uint materialIndex;
    uint firstIndex;
    float2 uvScale;
};

struct VSOutput
{
    float4 position     : SV_Position;
    float3 fragPosition : TEXCOORD0;
    float3 fragNormal   : TEXCOORD1;
    float2 fragTexCoord : TEXCOORD2;
    uint materialIndex;
};

[shader("vertex")]
VSOutput main(VSInput input)
{
    VSOutput o;

    float4 worldPos = mul(input.model, input.position);
    float4 viewPos  = mul(view, worldPos);
    o.position      = mul(proj, viewPos);

    o.fragPosition = worldPos.xyz;
    o.fragNormal   = normalize(mul(input.modelInverse, float4(input.normal.xyz, 0)).xyz);
    o.fragTexCoord = (input.texCoord.xy * input.uvScale);

    o.materialIndex = input.materialIndex;

    return o;
}