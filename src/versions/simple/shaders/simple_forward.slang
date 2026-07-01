/**
 * @file simple_forward.slang
 * @brief Vertex and fragment stages for the simple forward renderer.
 *
 * Vertex inputs: per-vertex position, color, and texture coordinate attributes.
 * Vertex constants: shared view/projection matrices in set 0 binding 0, plus one per-object model matrix as push constants.
 * Shadow output: depth-only clip position from the light view-projection matrix.
 * Fragment inputs: interpolated vertex color and world-space position from the vertex stage.
 * Fragment resources: directional light shadow map in set 0 binding 1 for hard shadowing.
 * Fragment outputs: one opaque RGBA color attachment value for a shadowed forward render pass.
 */

static const int kMaxShadowedSpotLights = 4; ///< Mirrors the simple engine fixed spot-shadow cap.
static const int kMaxDirectionalLights = 4;  ///< Mirrors the simple engine fixed directional-light cap.

struct FrameUniforms
{
    float4x4 view;       ///< Camera view matrix shared by all objects in the frame.
    float4x4 projection; ///< Camera projection matrix shared by all objects in the frame.
    float4x4 lightViewProj; ///< Light view-projection matrix for the upcoming shadow pass.
    float4x4 dirLightViewProj; ///< Directional-light view-projection matrix for future shadow data.
    float4x4 spotLightViewProjs[kMaxShadowedSpotLights]; ///< Spot-light view-projection matrices for future shadow data.
    float4 lightPositionRange;    ///< xyz world-space point-light position, w range.
    float4 lightColorIntensity;   ///< rgb direct-light color, w direct-light intensity.
    float4 lightShadowAmbient;    ///< xyz shadow-map direction approximation, w ambient contribution.
    float4 dirLightDirection;     ///< xyz world-space directional-light direction, w unused.
    float4 dirLightColorIntensity; ///< rgb directional-light color, w directional-light intensity.
    float4 dirLightShadowAmbient; ///< xyz directional shadow-map direction, w directional ambient contribution.
    float4 spotLightPositionRange; ///< xyz world-space spot-light position, w range.
    float4 spotLightColorIntensity; ///< rgb spot-light color, w spot-light intensity.
    float4 spotLightDirection;    ///< xyz spot-light light-to-scene direction, w unused.
    float4 spotLightConeAmbient;  ///< x inner cone cosine, y outer cone cosine, z active spot count, w ambient contribution.
    float4 spotLightPositionRanges[kMaxShadowedSpotLights]; ///< Per-spot xyz position with range in w.
    float4 spotLightColorIntensities[kMaxShadowedSpotLights]; ///< Per-spot rgb color with intensity in w.
    float4 spotLightDirections[kMaxShadowedSpotLights]; ///< Per-spot xyz light-to-scene direction with unused w.
    float4 spotLightConeAmbients[kMaxShadowedSpotLights]; ///< Per-spot inner cone, outer cone, active count, and ambient.
    float4 directionalLightDirections[kMaxDirectionalLights]; ///< Per-directional xyz light-to-scene direction with unused w.
    float4 directionalLightColorIntensities[kMaxDirectionalLights]; ///< Per-directional rgb color with intensity in w.
    float4 directionalLightAmbients[kMaxDirectionalLights]; ///< Per-directional ambient term packed in w.
    uint activeDirectionalLightCount; ///< Active directional-light count clamped to the fixed cap.
    uint3 directionalLightPadding;    ///< std140 padding after the scalar count.
};

struct ObjectPushConstants
{
    float4x4 model;              ///< Object-local model matrix selected before each draw call.
    uint useBaseColorTexture;    ///< Non-zero when the object wants the optional base-color texture.
    uint spotLightIndex;         ///< Shadow pass spot-light matrix index selected before each draw call.
};

struct VertexInput
{
    [[vk::location(0)]] float3 position : POSITION;  ///< Object-local vertex position.
    [[vk::location(1)]] float3 color    : COLOR0;    ///< Vertex color passed through to the fragment stage.
    [[vk::location(2)]] float2 texCoord : TEXCOORD0; ///< Texture coordinate kept for tutorial-compatible vertex layouts.
};

struct VertexOutput
{
    float4 position : SV_Position;                       ///< Clip-space position consumed by Vulkan.
    [[vk::location(0)]] float3 color    : COLOR0;         ///< Interpolated vertex color for simple forward output.
    [[vk::location(1)]] float3 worldPos : TEXCOORD0;      ///< Interpolated world-space position for flat lighting.
    [[vk::location(2)]] float2 texCoord : TEXCOORD1;      ///< Interpolated texture coordinate for later material sampling.
};

struct FragmentInput
{
    [[vk::location(0)]] float3 color    : COLOR0;    ///< Interpolated vertex color.
    [[vk::location(1)]] float3 worldPos : TEXCOORD0; ///< Interpolated world-space position.
    [[vk::location(2)]] float2 texCoord : TEXCOORD1; ///< Interpolated texture coordinate for later material sampling.
};

struct FragmentOutput
{
    [[vk::location(0)]] float4 color : SV_Target0; ///< Opaque color written to the framebuffer.
};

[[vk::binding(0, 0)]]
ConstantBuffer<FrameUniforms> frame; ///< Frame-wide camera constants.

[[vk::binding(1, 0)]]
Sampler2D shadowMap; ///< Directional light depth map sampled by the forward fragment stage.

[[vk::binding(2, 0)]]
Sampler2D baseColorTexture; ///< Optional object base-color texture sampled only when the push constant enables it.

[[vk::binding(3, 0)]]
Sampler2D dirShadowMap; ///< Directional light depth map sampled by the forward fragment stage.

[[vk::binding(4, 0)]]
Sampler2D spotShadowMap; ///< Spot light depth map sampled by the forward fragment stage.

[[vk::binding(5, 0)]]
Sampler2DArray spotShadowArray; ///< Spot light depth array sampled by the forward fragment stage.

[[vk::push_constant]]
ObjectPushConstants object; ///< Per-object transform constants.

[shader("vertex")]
VertexOutput vertexMain(VertexInput input)
{
    VertexOutput output;
    const float4 worldPosition = mul(object.model, float4(input.position, 1.0));
    const float4 viewPosition = mul(frame.view, worldPosition);
    output.position = mul(frame.projection, viewPosition);
    output.color = input.color;
    output.worldPos = worldPosition.xyz;
    output.texCoord = input.texCoord; ///< Preserve vertex UVs for the fragment interface.
    return output;
}

[shader("vertex")]
float4 shadowVertexMain(VertexInput input) : SV_Position
{
    return mul(frame.lightViewProj, mul(object.model, float4(input.position, 1.0)));
}

[shader("vertex")]
float4 shadowVertexMainDir(VertexInput input) : SV_Position
{
    return mul(frame.dirLightViewProj, mul(object.model, float4(input.position, 1.0)));
}

[shader("vertex")]
float4 shadowVertexMainSpot(VertexInput input) : SV_Position
{
    return mul(frame.spotLightViewProjs[object.spotLightIndex], mul(object.model, float4(input.position, 1.0)));
}

[shader("fragment")]
FragmentOutput fragmentMain(FragmentInput input)
{
    FragmentOutput output;
    const float ambient = frame.lightShadowAmbient.w;                    ///< Base light keeps back faces visible.
    const float shininess = 32.0;                                        ///< Fixed Blinn-Phong highlight exponent.
    const float specularStrength = 0.3;                                  ///< Modest highlight strength for colored surfaces.
    const float3 lightColor = frame.lightColorIntensity.rgb;             ///< Scene-selected direct-light color.
    const float lightIntensity = frame.lightColorIntensity.w;            ///< Scene-selected direct-light strength.
    const float3 rawNormal = normalize(cross(ddx(input.worldPos), ddy(input.worldPos))); ///< Flat geometric face normal from screen derivatives.
    const float3 lightOffset = frame.lightPositionRange.xyz - input.worldPos; ///< Vector from fragment to the point light.
    const float distanceToLight = length(lightOffset);                  ///< Fragment-light distance for falloff.
    const float3 surfaceLightDir = lightOffset / max(distanceToLight, 0.001); ///< Point-light direction changes across the scene.
    const float lightRange = max(frame.lightPositionRange.w, 0.001);    ///< Avoid division by zero for malformed scene data.
    const float rangeFalloff = saturate(1.0 - (distanceToLight / lightRange)); ///< Fade direct light at the configured range.
    const float attenuation = rangeFalloff * rangeFalloff;              ///< Smooth enough quadratic falloff for the debug scene.
    const float3 viewOffset = float3(frame.view[0][3], frame.view[1][3], frame.view[2][3]); ///< View translation from the existing matrix.
    const float3 cameraPos = -float3(
        dot(float3(frame.view[0][0], frame.view[1][0], frame.view[2][0]), viewOffset),
        dot(float3(frame.view[0][1], frame.view[1][1], frame.view[2][1]), viewOffset),
        dot(float3(frame.view[0][2], frame.view[1][2], frame.view[2][2]), viewOffset)); ///< World eye from inverse rigid view transform.
    const float3 viewDir = normalize(cameraPos - input.worldPos);        ///< Surface-to-eye vector for view-dependent highlights.
    const float3 normal = dot(rawNormal, viewDir) < 0.0 ? -rawNormal : rawNormal; ///< Keep visible debug faces lit even when winding flips derivative normals.
    const float diffuse = max(dot(normal, surfaceLightDir), 0.0);        ///< Lambertian light response.
    const float3 halfVector = normalize(surfaceLightDir + viewDir);      ///< Blinn-Phong halfway direction between light and eye.
    const float specular = specularStrength * pow(max(dot(normal, halfVector), 0.0), shininess); ///< Shadowed highlight response.
    const float4 dirLightClip = mul(frame.dirLightViewProj, float4(input.worldPos, 1.0)); ///< Fragment position in directional light clip space.
    const float3 dirLightNdc = dirLightClip.xyz / dirLightClip.w;        ///< Directional light depth remains in 0..1 after divide.
    const float2 dirShadowUv = (dirLightNdc.xy * 0.5) + 0.5;             ///< Convert directional light NDC XY to shadow texture UVs.
    const bool insideDirShadowMap = dirShadowUv.x >= 0.0 && dirShadowUv.x <= 1.0 &&
        dirShadowUv.y >= 0.0 && dirShadowUv.y <= 1.0 && dirLightNdc.z >= 0.0 && dirLightNdc.z <= 1.0; ///< Reject fragments outside directional light coverage.
    const float dirStoredDepth = dirShadowMap.Sample(dirShadowUv).r;     ///< Closest surface depth from the directional light pass.
    const float dirShadowFactor = insideDirShadowMap && dirLightNdc.z - 0.001 > dirStoredDepth ? 0.35 : 1.0; ///< Small 0.001 compare bias complements shadow-pass slope-scaled rasterizer bias.
    const int activeDirectionalLightCount = min(max(int(frame.activeDirectionalLightCount), 0), kMaxDirectionalLights); ///< Clamp scene directional count to the fixed array.
    float3 directionalContribution = float3(0.0);                  ///< Accumulated directional-light response for all active directional lights.
    for (int i = 0; i < kMaxDirectionalLights; ++i) {              ///< Fixed trip count lets the compiler unroll the small directional loop.
        if (i < activeDirectionalLightCount) {                     ///< Ignore unused directional-light slots.
            const float3 dirSurfaceLightDir = normalize(-frame.directionalLightDirections[i].xyz); ///< Directional surface-to-light vector.
            const float dirDiffuse = max(dot(normal, dirSurfaceLightDir), 0.0); ///< Directional Lambertian light response.
            const float3 dirHalfVector = normalize(dirSurfaceLightDir + viewDir); ///< Directional Blinn-Phong halfway direction.
            const float dirSpecular = specularStrength * pow(max(dot(normal, dirHalfVector), 0.0), shininess); ///< Directional highlight response.
            const float3 dirLight = frame.directionalLightColorIntensities[i].rgb * frame.directionalLightColorIntensities[i].w; ///< Directional light color scaled by intensity.
            const float dirAmbient = frame.directionalLightAmbients[i].w; ///< Directional ambient contribution without shadowing.
            const float activeDirShadowFactor = i == 0 ? dirShadowFactor : 1.0; ///< Slot zero keeps the existing directional shadow map.
            directionalContribution += dirAmbient + dirLight * ((dirDiffuse + dirSpecular) * activeDirShadowFactor); ///< Add this directional result.
        }
    }
    const int activeSpotLightCount = min(max(int(frame.spotLightConeAmbient.z), 0), kMaxShadowedSpotLights); ///< Clamp scene spot count to the fixed shadow array.
    float3 spotContribution = float3(0.0);                           ///< Accumulated spot-light response for all active shadowed spots.
    for (int i = 0; i < kMaxShadowedSpotLights; ++i) {                ///< Fixed trip count lets the compiler unroll the small spot loop.
        if (i < activeSpotLightCount) {                               ///< Ignore unused shadow-array layers.
            const float3 spotToLight = frame.spotLightPositionRanges[i].xyz - input.worldPos; ///< Vector from fragment to the spot light.
            const float spotDistance = length(spotToLight);           ///< Fragment-spot distance for falloff.
            const float3 spotLightDir = spotToLight / max(spotDistance, 0.001); ///< Spot surface-to-light vector for direct response.
            const float spotRange = max(frame.spotLightPositionRanges[i].w, 0.001); ///< Positive spot range for stable attenuation.
            const float spotRangeValid = frame.spotLightPositionRanges[i].w > 0.0 ? 1.0 : 0.0; ///< Disable spot light for invalid ranges.
            const float spotRangeFalloff = saturate(1.0 - (spotDistance / spotRange)) * spotRangeValid; ///< Fade spot light at the configured range.
            const float spotAttenuation = spotRangeFalloff * spotRangeFalloff; ///< Spot quadratic distance falloff.
            const float spotDiffuse = max(dot(normal, spotLightDir), 0.0); ///< Spot Lambertian light response.
            const float3 spotHalfVector = normalize(spotLightDir + viewDir); ///< Spot Blinn-Phong halfway direction.
            const float spotSpecular = specularStrength * pow(max(dot(normal, spotHalfVector), 0.0), shininess); ///< Spot highlight response.
            const float spotCos = dot(normalize(-spotLightDir), normalize(frame.spotLightDirections[i].xyz)); ///< Spot angle cosine for cone falloff.
            const float spotConeFalloff = smoothstep(frame.spotLightConeAmbients[i].y, frame.spotLightConeAmbients[i].x, spotCos); ///< Spot cone angular falloff.
            const float3 spotLight = frame.spotLightColorIntensities[i].rgb * frame.spotLightColorIntensities[i].w; ///< Spot light color scaled by intensity.
            const float spotAmbient = frame.spotLightConeAmbients[i].w; ///< Spot ambient contribution without shadowing.
            const float4 spotLightClip = mul(frame.spotLightViewProjs[i], float4(input.worldPos, 1.0)); ///< Fragment position in spot light clip space.
            const float3 spotLightNdc = spotLightClip.xyz / spotLightClip.w; ///< Spot light depth remains in 0..1 after divide.
            const float2 spotShadowUv = (spotLightNdc.xy * 0.5) + 0.5; ///< Convert spot light NDC XY to shadow texture UVs.
            const bool insideSpotShadowMap = spotShadowUv.x >= 0.0 && spotShadowUv.x <= 1.0 &&
                spotShadowUv.y >= 0.0 && spotShadowUv.y <= 1.0 && spotLightNdc.z >= 0.0 && spotLightNdc.z <= 1.0; ///< Reject fragments outside spot light coverage.
            const float spotStoredDepth = spotShadowArray.Sample(float3(spotShadowUv, float(i))).r; ///< Closest surface depth from this spot layer.
            const float spotShadowFactor = insideSpotShadowMap && spotLightNdc.z - 0.001 > spotStoredDepth ? 0.35 : 1.0; ///< Small 0.001 compare bias complements shadow-pass slope-scaled rasterizer bias.
            spotContribution += spotAmbient + spotLight * ((spotDiffuse + spotSpecular) * spotAttenuation * spotConeFalloff * spotShadowFactor); ///< Add this cone-attenuated spot result.
        }
    }
    const float4 lightClip = mul(frame.lightViewProj, float4(input.worldPos, 1.0)); ///< Fragment position in light clip space.
    const float3 lightNdc = lightClip.xyz / lightClip.w;                ///< Vulkan light depth remains in 0..1 after divide.
    const float2 shadowUv = (lightNdc.xy * 0.5) + 0.5;                  ///< Convert light NDC XY to shadow texture UVs.
    const bool insideShadowMap = shadowUv.x >= 0.0 && shadowUv.x <= 1.0 &&
        shadowUv.y >= 0.0 && shadowUv.y <= 1.0 && lightNdc.z >= 0.0 && lightNdc.z <= 1.0; ///< Reject fragments outside light coverage.
    const float storedDepth = shadowMap.Sample(shadowUv).r;             ///< Closest surface depth from the light pass.
    const float shadowFactor = insideShadowMap && lightNdc.z - 0.001 > storedDepth ? 0.35 : 1.0; ///< Small 0.001 compare bias complements shadow-pass slope-scaled rasterizer bias.
    float3 baseColor = input.color;                                      ///< Vertex color remains the default material color.
    if (object.useBaseColorTexture != 0) {
        baseColor *= baseColorTexture.Sample(input.texCoord).rgb;        ///< Optional texture modulates the material before lighting.
    }
    output.color = float4(baseColor * (ambient + lightColor * ((diffuse + specular) * lightIntensity * attenuation * shadowFactor) + directionalContribution + spotContribution), 1.0); ///< Apply point, directional, and spot Blinn-Phong.
    return output;
}
