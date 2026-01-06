import Materials;
import Frame;

struct FSInput
{
    float3 fragPosition : TEXCOORD0;
    float3 fragNormal : TEXCOORD1;
    float3 fragTangent : TEXCOORD2;
    float2 fragTexCoord : TEXCOORD3;
    uint materialIndex;
};

[[vk::binding(1, 0)]]
StructuredBuffer<Material> materials;

[[vk::binding(2, 0)]]
Sampler2D textures[];

struct FSOutput
{
    float4 outAlbedoAlpha : SV_Target0;
    float3 outNormal      : SV_Target1;
    float3 outSpec        : SV_Target2;
    float3 outPosition    : SV_Target3;
    float3 outShadingNormal : SV_Target4;
};


[shader("pixel")]
FSOutput main(FSInput input)
{
    FSOutput o;

    Material mat = materials[input.materialIndex];

    Frame geometryFrame = Frame(input.fragNormal, input.fragTangent);

    MaterialRecord material = getMaterialProperties(mat, textures, input.fragTexCoord, geometryFrame);

    o.outAlbedoAlpha = float4(material.albedo, material.alpha);

    o.outNormal = input.fragNormal;
    o.outShadingNormal = material.shadingFrame.n;
    o.outSpec = float3(material.roughness, material.metalness, material.ior);
    o.outPosition = input.fragPosition;

    return o;
}