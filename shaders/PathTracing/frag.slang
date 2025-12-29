import Materials;


struct FSInput
{
    float3 fragPosition : TEXCOORD0;
    float3 fragNormal   : TEXCOORD1;
    float2 fragTexCoord : TEXCOORD2;
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
};


[shader("pixel")]
FSOutput main(FSInput input)
{
    FSOutput o;

    Material mat = materials[input.materialIndex];

    //o.outAlbedoAlpha = textures[mat.albedoTextureIndex].Sample(input.fragTexCoord);

    o.outAlbedoAlpha = float4(getAlbedo(mat, textures, input.fragTexCoord), getAlpha(mat, textures, input.fragTexCoord));

    o.outNormal = input.fragNormal;
    o.outSpec = float3(getRoughness(mat, textures, input.fragTexCoord), getMetallic(mat, textures, input.fragTexCoord), getIOR(mat, textures, input.fragTexCoord));
    o.outPosition = input.fragPosition;

    return o;
}