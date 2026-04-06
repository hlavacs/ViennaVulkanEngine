/// @file Rasterizer shader entry points and shared parameter blocks for v3.
#include "lights.slang"

struct VertexInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float2 texCoord : TEXCOORD0;
    float4 color : COLOR0;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float3 worldPosition : TEXCOORD0;
    float3 worldNormal : TEXCOORD1;
    float3 worldTangent : TEXCOORD2;
    float2 texCoord : TEXCOORD3;
    float4 color : COLOR0;
};

struct FrameConstants
{
    float4x4 model;
    float4x4 view;
    float4x4 projection;
};

struct MaterialConstants
{
    float4 baseColorFactor;
    float normalScale;
};

struct RasterizerParams
{
    ConstantBuffer<FrameConstants> frame;
    ConstantBuffer<MaterialConstants> material;
    ConstantBuffer<LightingConstants> lighting;
    Texture2D<float4> baseColorTexture;
    SamplerState baseColorSampler;
    Texture2D<float4> normalTexture;
    SamplerState normalSampler;
};

ParameterBlock<RasterizerParams> gRasterizerParams;

float3 sampleWorldNormal(VertexOutput input)
{
    float3 worldNormal = normalize(input.worldNormal);
    float3 worldTangent = normalize(input.worldTangent);
    float3 worldBitangent = normalize(cross(worldNormal, worldTangent));

    float3 sampledNormal =
        gRasterizerParams.normalTexture.Sample(gRasterizerParams.normalSampler, input.texCoord).xyz
        * 2.0
        - 1.0;
    sampledNormal.xy *= gRasterizerParams.material.normalScale;

    float xyLength = dot(sampledNormal.xy, sampledNormal.xy);
    sampledNormal.z = sqrt(saturate(1.0 - xyLength));

    float3x3 tbn = float3x3(worldTangent, worldBitangent, worldNormal);
    return normalize(mul(tbn, sampledNormal));
}

[shader("vertex")]
VertexOutput vertexMain(VertexInput input)
{
    VertexOutput output;

    float4 worldPosition = mul(gRasterizerParams.frame.model, float4(input.position, 1.0));
    float4 viewPosition = mul(gRasterizerParams.frame.view, worldPosition);
    output.position = mul(gRasterizerParams.frame.projection, viewPosition);

    float3x3 normalMatrix = (float3x3)gRasterizerParams.frame.model;
    output.worldPosition = worldPosition.xyz;
    output.worldNormal = normalize(mul(normalMatrix, input.normal));
    output.worldTangent = normalize(mul(normalMatrix, input.tangent));
    output.texCoord = input.texCoord;
    output.color = input.color;

    return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target
{
    float3 normal = sampleWorldNormal(input);
    float3 lighting = gRasterizerParams.lighting.ambientColor;

    uint activeLightCount = min(gRasterizerParams.lighting.lightCount, kMaxLights);
    for (uint lightIndex = 0; lightIndex < activeLightCount; ++lightIndex)
    {
        LightData light = gRasterizerParams.lighting.lights[lightIndex];
        lighting += evaluateLight(light, input.worldPosition, normal);
    }

    float4 albedo = gRasterizerParams.baseColorTexture.Sample(
        gRasterizerParams.baseColorSampler,
        input.texCoord);
    float4 surfaceColor = albedo * gRasterizerParams.material.baseColorFactor * input.color;

    return float4(surfaceColor.rgb * lighting, surfaceColor.a);
}
