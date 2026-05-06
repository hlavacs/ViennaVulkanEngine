/// @file First v4 forward shader with baked-in debug outputs for verification.
#include "Lighting.slang"

static const uint VVE_MATERIAL_HAS_BASE_COLOR_TEXTURE = 1; ///< Material flag for base-color sampling.

/// @brief Vertex format expected by the first educational forward renderer.
struct VveForwardVertexInput
{
    float3 position : POSITION;  ///< Object-space vertex position.
    float3 normal : NORMAL;      ///< Object-space vertex normal.
    float2 uv : TEXCOORD0;       ///< Base material UV coordinate.
};

/// @brief Interpolated vertex data consumed by the fragment shader.
struct VveForwardVertexOutput
{
    float4 clipPosition : SV_Position; ///< Clip-space position; pixel-space in fragment input.
    float3 worldPosition : TEXCOORD0;  ///< World-space position for lighting/debug readback.
    float3 worldNormal : TEXCOORD1;    ///< World-space normal for lighting/debug readback.
    float2 uv : TEXCOORD2;             ///< Material UV coordinate.
};

/// @brief Per-frame matrices and camera data.
struct VveForwardFrameConstants
{
    float4x4 view;       ///< World-to-view transform.
    float4x4 projection; ///< View-to-clip transform.
    float3 cameraPosition; ///< World-space camera position.
    float timeSeconds;   ///< Frame time for deterministic animated debug scenes.
};

/// @brief Per-object transform data.
struct VveForwardObjectConstants
{
    float4x4 model;        ///< Object-to-world transform.
    float4x4 normalMatrix; ///< Object-normal-to-world transform.
    uint objectId;         ///< Stable renderer-local object id for debug samples.
    uint materialId;       ///< Stable renderer-local material id for debug samples.
    uint padding0;         ///< Keeps buffer layout explicit.
    uint padding1;         ///< Keeps buffer layout explicit.
};

/// @brief Per-material data for the first forward pass.
struct VveForwardMaterialConstants
{
    float4 baseColorFactor; ///< Linear RGBA multiplier.
    uint flags;             ///< Bit flags such as VVE_MATERIAL_HAS_BASE_COLOR_TEXTURE.
    float alphaCutoff;      ///< Alpha-test threshold; negative disables alpha test.
    float padding0;         ///< Keeps buffer layout explicit.
    float padding1;         ///< Keeps buffer layout explicit.
};

/// @brief One exact debug sample requested by host-side verification code.
struct VveForwardDebugRequest
{
    uint2 pixel;       ///< Pixel coordinate to capture.
    uint sampleIndex;  ///< Output index in gVveForward.debugSamples.
    uint enabled;      ///< Non-zero enables this request.
};

/// @brief Exact per-pixel shader data copied back to the host for CPU/GPU comparison.
struct VveForwardDebugSample
{
    uint2 pixel;               ///< Captured pixel coordinate.
    uint objectId;             ///< Renderer-local object id.
    uint materialId;           ///< Renderer-local material id.
    float3 worldPosition;      ///< Fragment world-space position.
    float depth;               ///< Fragment depth from SV_Position.z.
    float3 worldNormal;        ///< Fragment world-space normal.
    float nDotL;               ///< Lambert cosine term.
    float4 lightClipPosition;  ///< Fragment position in light clip space.
    float3 shadowCoord;        ///< Shadow-map UV/depth coordinates.
    float shadowBias;          ///< Bias used for depth comparison.
    float shadowFactor;        ///< Shadow factor used by final lighting.
    float3 ambient;            ///< Ambient lighting contribution.
    float alpha;               ///< Final alpha.
    float3 diffuse;            ///< Direct diffuse lighting contribution.
    float exposure;            ///< Applied exposure.
    float3 finalColor;         ///< Final linear RGB before presentation.
    float padding0;            ///< Keeps buffer layout explicit.
};

/// @brief Reflection-friendly parameter block for the forward pass.
struct VveForwardParams
{
    ConstantBuffer<VveForwardFrameConstants> frame;       ///< Per-frame constants.
    ConstantBuffer<VveForwardObjectConstants> object;     ///< Per-object constants.
    ConstantBuffer<VveForwardMaterialConstants> material; ///< Per-material constants.
    ConstantBuffer<VveLightingConstants> lighting;        ///< Lighting/debug constants.
    Texture2D<float4> baseColorTexture;                   ///< Optional base-color texture.
    SamplerState baseColorSampler;                        ///< Sampler for baseColorTexture.
    StructuredBuffer<VveForwardDebugRequest> debugRequests; ///< Host-requested sample pixels.
    RWStructuredBuffer<VveForwardDebugSample> debugSamples; ///< Host-readable debug sample output.
};

ParameterBlock<VveForwardParams> gVveForward; ///< Single reflected parameter block for this pass.

/// @brief Returns whether the current material should sample the base-color texture.
bool vveMaterialUsesBaseColorTexture()
{
    return (gVveForward.material.flags & VVE_MATERIAL_HAS_BASE_COLOR_TEXTURE) != 0;
}

/// @brief Builds the surface payload shared by lighting and debug output.
VveSurfaceData vveBuildSurface(VveForwardVertexOutput input)
{
    float4 baseColor = gVveForward.material.baseColorFactor;
    if (vveMaterialUsesBaseColorTexture())
    {
        baseColor *= gVveForward.baseColorTexture.Sample(gVveForward.baseColorSampler, input.uv);
    }

    VveSurfaceData surface;
    surface.worldPosition = input.worldPosition;
    surface.worldNormal = vveSafeNormalize(input.worldNormal);
    surface.uv = input.uv;
    surface.baseColor = baseColor.rgb;
    surface.alpha = baseColor.a;
    return surface;
}

/// @brief Writes one requested exact debug sample when the fragment matches a requested pixel.
void vveWriteDebugSample(VveForwardVertexOutput input,
                         VveSurfaceData surface,
                         VveLightingResult lighting,
                         float3 finalColor)
{
    if ((gVveForward.lighting.debugMode & VVE_DEBUG_MODE_SAMPLES) == 0) { return; }

    uint2 pixel = uint2(input.clipPosition.xy);
    uint requestCount = min(gVveForward.lighting.debugSampleCount, VVE_MAX_DEBUG_SAMPLES);
    for (uint index = 0; index < requestCount; ++index)
    {
        VveForwardDebugRequest request = gVveForward.debugRequests[index];
        if (request.enabled == 0 || any(request.pixel != pixel)) { continue; }

        float4 lightClipPosition = vveComputeLightClipPosition(gVveForward.lighting, surface.worldPosition);
        VveForwardDebugSample sample;
        sample.pixel = pixel;
        sample.objectId = gVveForward.object.objectId;
        sample.materialId = gVveForward.object.materialId;
        sample.worldPosition = surface.worldPosition;
        sample.depth = input.clipPosition.z;
        sample.worldNormal = surface.worldNormal;
        sample.nDotL = lighting.nDotL;
        sample.lightClipPosition = lightClipPosition;
        sample.shadowCoord = vveLightClipToShadowCoord(lightClipPosition);
        sample.shadowBias = gVveForward.lighting.shadowBias;
        sample.shadowFactor = lighting.shadowFactor;
        sample.ambient = lighting.ambient;
        sample.alpha = surface.alpha;
        sample.diffuse = lighting.diffuse;
        sample.exposure = gVveForward.lighting.exposure;
        sample.finalColor = finalColor;
        sample.padding0 = 0.0;
        gVveForward.debugSamples[request.sampleIndex] = sample;
    }
}

/// @brief Multiple render targets used by the debug verification path.
struct VveForwardFragmentOutput
{
    float4 color : SV_Target0;         ///< Final linear color.
    float4 worldPosition : SV_Target1; ///< Debug world position.
    float4 worldNormal : SV_Target2;   ///< Debug encoded normal plus nDotL.
    float4 lighting : SV_Target3;      ///< Debug ambient/diffuse/shadow summary.
    float4 shadow : SV_Target4;        ///< Debug shadow coordinates and factor.
};

[shader("vertex")]
VveForwardVertexOutput vveForwardVertexMain(VveForwardVertexInput input)
{
    float4 worldPosition = mul(gVveForward.object.model, float4(input.position, 1.0));

    VveForwardVertexOutput output;
    output.clipPosition = mul(gVveForward.frame.projection, mul(gVveForward.frame.view, worldPosition));
    output.worldPosition = worldPosition.xyz;
    output.worldNormal = vveSafeNormalize(mul((float3x3)gVveForward.object.normalMatrix, input.normal));
    output.uv = input.uv;
    return output;
}

[shader("fragment")]
VveForwardFragmentOutput vveForwardFragmentMain(VveForwardVertexOutput input)
{
    VveSurfaceData surface = vveBuildSurface(input);
    if (gVveForward.material.alphaCutoff >= 0.0 && surface.alpha < gVveForward.material.alphaCutoff)
    {
        discard;
    }

    VveLightingResult lighting = vveEvaluateDirectionalLight(gVveForward.lighting, surface);
    float3 finalColor = surface.baseColor * lighting.finalLighting * gVveForward.lighting.exposure;
    float4 lightClipPosition = vveComputeLightClipPosition(gVveForward.lighting, surface.worldPosition);
    float3 shadowCoord = vveLightClipToShadowCoord(lightClipPosition);
    vveWriteDebugSample(input, surface, lighting, finalColor);

    VveForwardFragmentOutput output;
    output.color = float4(finalColor, surface.alpha);
    output.worldPosition = float4(surface.worldPosition, 1.0);
    output.worldNormal = float4(vveEncodeSignedUnit(surface.worldNormal), lighting.nDotL);
    output.lighting = float4(lighting.diffuse, lighting.shadowFactor);
    output.shadow = float4(shadowCoord.xy, shadowCoord.z, lighting.shadowFactor);
    return output;
}
