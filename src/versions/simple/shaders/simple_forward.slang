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

static const int kMaxShadowedSpotLights = 10; ///< Mirrors the simple engine fixed spot-shadow cap.
static const int kMaxShadowedPointLights = 10; ///< Mirrors the simple engine fixed point-shadow cap.
static const int kMaxDirectionalLights = 4;   ///< Mirrors the simple engine fixed directional-light cap.
static const int kNumShadowCascades = 4;      ///< Mirrors the fixed cascade count used by directional shadow resources.
static const int kShadowMapResolution = 1024; ///< Mirrors ShadowMap::resolution for software PCF offsets.
static const bool kDebugCascadeTint = false;  ///< Enables a visible red/green/blue/yellow directional-cascade overlay.

struct FrameUniforms
{
    float4x4 view;       ///< Camera view matrix shared by all objects in the frame.
    float4x4 projection; ///< Camera projection matrix shared by all objects in the frame.
    float4x4 lightViewProj; ///< Light view-projection matrix for the upcoming shadow pass.
    float4x4 dirLightViewProjArray[kMaxDirectionalLights * kNumShadowCascades]; ///< Flattened per-light cascade matrices for shadow depth passes.
    float4 cascadeSplits; ///< View-space far distance of each directional shadow cascade.
    float4x4 spotLightViewProjs[kMaxShadowedSpotLights]; ///< Spot-light view-projection matrices for future shadow data.
    float4x4 pointLightFaceViewProjs[kMaxShadowedPointLights * 6]; ///< Point-light face view-projection matrices for future shadow data.
    float4 pointLightPositionRanges[kMaxShadowedPointLights]; ///< Per-point xyz position with range in w.
    float4 pointLightColorIntensities[kMaxShadowedPointLights]; ///< Per-point rgb color with intensity in w.
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
    uint dirLightIndex;          ///< Shadow pass directional-light matrix index selected before each draw call.
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

[[vk::binding(4, 0)]]
Sampler2D spotShadowMap; ///< Spot light depth map sampled by the forward fragment stage.

[[vk::binding(5, 0)]]
Sampler2DArrayShadow spotShadowArray; ///< Spot light comparison-depth array sampled by the forward fragment stage.

[[vk::binding(6, 0)]]
Sampler2DArrayShadow dirShadowArray; ///< Directional light comparison-depth array sampled per active light by the fragment stage.

[[vk::binding(7, 0)]]
Sampler2DArrayShadow pointShadowArray; ///< Point light comparison-depth array sampled per cubemap-style face.

[[vk::push_constant]]
ObjectPushConstants object; ///< Per-object transform constants.

/**
 * @brief Converts a fixed world-space shadow offset to nonlinear Vulkan perspective depth.
 */
float perspectiveShadowBias(float farPlane, float axialDepth)
{
    const float nearPlane = 0.1; ///< Mirrors the CPU perspective-shadow projection near plane.
    const float worldBias = 0.02; ///< Keep perspective-shadow detachment bounded to two centimetres.
    const float biasedDepth = max(axialDepth, nearPlane + worldBias); ///< Guard the reciprocal near the light origin.
    const float depthScale = (farPlane * nearPlane) / max(farPlane - nearPlane, 0.001); ///< Projection reciprocal coefficient.
    return max(depthScale * worldBias / (biasedDepth * max(biasedDepth - worldBias, 0.001)), 0.000002);
}

/**
 * @brief Averages a 3x3 grid of hardware-filtered depth comparisons.
 */
float pcfShadow(Sampler2DArrayShadow shadowArray, float2 uv, int layer, float referenceDepth)
{
    const float texelSize = 1.0 / float(kShadowMapResolution); ///< One shadow texel in normalized coordinates.
    float visibility = 0.0; ///< Accumulated lit fraction from nine comparison taps.
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            const float2 offset = float2(float(x), float(y)) * texelSize; ///< Neighboring comparison location.
            visibility += shadowArray.SampleCmpLevelZero(float3(uv + offset, float(layer)), referenceDepth);
        }
    }
    return visibility / 9.0;
}

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
    return mul(frame.dirLightViewProjArray[object.dirLightIndex], mul(object.model, float4(input.position, 1.0))); ///< Select the directional light-space matrix for this pass.
}

[shader("vertex")]
float4 shadowVertexMainSpot(VertexInput input) : SV_Position
{
    return mul(frame.spotLightViewProjs[object.spotLightIndex], mul(object.model, float4(input.position, 1.0)));
}

[shader("vertex")]
float4 shadowVertexMainPoint(VertexInput input) : SV_Position
{
    return mul(frame.pointLightFaceViewProjs[object.spotLightIndex], mul(object.model, float4(input.position, 1.0))); ///< Select the point-light face matrix with the spot-pass draw selector.
}

[shader("fragment")]
FragmentOutput fragmentMain(FragmentInput input)
{
    FragmentOutput output;
    const float ambient = frame.lightShadowAmbient.w;                    ///< Base light keeps back faces visible.
    const float shininess = 32.0;                                        ///< Fixed Blinn-Phong highlight exponent.
    const float specularStrength = 0.3;                                  ///< Modest highlight strength for colored surfaces.
    const float3 rawNormal = normalize(cross(ddx(input.worldPos), ddy(input.worldPos))); ///< Flat geometric face normal from screen derivatives.
    const float3 viewOffset = float3(frame.view[0][3], frame.view[1][3], frame.view[2][3]); ///< View translation from the existing matrix.
    const float3 cameraPos = -float3(
        dot(float3(frame.view[0][0], frame.view[1][0], frame.view[2][0]), viewOffset),
        dot(float3(frame.view[0][1], frame.view[1][1], frame.view[2][1]), viewOffset),
        dot(float3(frame.view[0][2], frame.view[1][2], frame.view[2][2]), viewOffset)); ///< World eye from inverse rigid view transform.
    const float3 viewDir = normalize(cameraPos - input.worldPos);        ///< Surface-to-eye vector for view-dependent highlights.
    const float3 normal = dot(rawNormal, viewDir) < 0.0 ? -rawNormal : rawNormal; ///< Keep visible debug faces lit even when winding flips derivative normals.
    float3 pointContribution = float3(0.0);                              ///< Accumulated point-light response for active point slots.
    for (int i = 0; i < kMaxShadowedPointLights; ++i) {                  ///< Fixed trip count keeps point lights aligned with the uploaded arrays.
        const float pointRangeRaw = frame.pointLightPositionRanges[i].w; ///< Zero range marks an inactive point-light slot.
        const float pointIntensity = frame.pointLightColorIntensities[i].w; ///< Zero intensity marks an inactive point-light slot.
        if (pointRangeRaw > 0.0 && pointIntensity > 0.0) {               ///< Skip inactive slots before any range division.
            const float3 pointOffset = frame.pointLightPositionRanges[i].xyz - input.worldPos; ///< Vector from fragment to this point light.
            const float pointDistance = length(pointOffset);             ///< Fragment-light distance for falloff.
            const float3 pointLightDir = pointOffset / max(pointDistance, 0.001); ///< Point-light direction changes across the scene.
            const float pointRangeFalloff = saturate(1.0 - (pointDistance / pointRangeRaw)); ///< Fade direct light at the configured range.
            const float pointAttenuation = pointRangeFalloff * pointRangeFalloff; ///< Smooth enough quadratic falloff for the debug scene.
            const float3 pointNormal = dot(rawNormal, pointLightDir) < 0.0 ? -rawNormal : rawNormal; ///< Two-sided normal faces the point light, not the camera.
            const float pointDiffuse = max(dot(pointNormal, pointLightDir), 0.0); ///< Lambertian response now varies with point-light position.
            const float3 pointHalfVector = normalize(pointLightDir + viewDir); ///< Blinn-Phong halfway direction between light and eye.
            const float pointSpecular = specularStrength * pow(max(dot(pointNormal, pointHalfVector), 0.0), shininess); ///< Point-light highlight response.
            const float3 pointLight = frame.pointLightColorIntensities[i].rgb * pointIntensity; ///< Point light color scaled by intensity.
            const float3 pointLightToFragment = input.worldPos - frame.pointLightPositionRanges[i].xyz; ///< Vector selects the cubemap-style shadow face.
            const float3 pointFaceAbs = abs(pointLightToFragment); ///< Dominant axis chooses the CPU face order.
            int pointFace = pointFaceAbs.x >= pointFaceAbs.y && pointFaceAbs.x >= pointFaceAbs.z ? (pointLightToFragment.x >= 0.0 ? 0 : 1) :
                (pointFaceAbs.y >= pointFaceAbs.z ? (pointLightToFragment.y >= 0.0 ? 2 : 3) : (pointLightToFragment.z >= 0.0 ? 4 : 5)); ///< Face order: +X, -X, +Y, -Y, +Z, -Z.
            const int pointShadowLayer = i * 6 + pointFace; ///< Dense depth-array layer for this point-light face.
            const float4 pointLightClip = mul(frame.pointLightFaceViewProjs[pointShadowLayer], float4(input.worldPos, 1.0)); ///< Fragment in selected point-face clip space.
            const float pointClipW = abs(pointLightClip.w) > 0.001 ? pointLightClip.w : 0.001; ///< Avoid invalid perspective division at the light origin.
            const float3 pointLightNdc = pointLightClip.xyz / pointClipW; ///< Point face depth remains in 0..1 after divide.
            const float2 pointShadowUv = (pointLightNdc.xy * 0.5) + 0.5; ///< Convert point face NDC XY to shadow texture UVs.
            const bool insidePointShadowMap = pointLightClip.w > 0.001 && pointShadowUv.x >= 0.0 && pointShadowUv.x <= 1.0 &&
                pointShadowUv.y >= 0.0 && pointShadowUv.y <= 1.0 && pointLightNdc.z >= 0.0 && pointLightNdc.z <= 1.0; ///< Reject fragments outside the selected point face.
            const float pointShadowBias = perspectiveShadowBias(pointRangeRaw, pointLightClip.w); ///< Convert the world offset at this face-axis distance.
            const float pointShadowPcf = insidePointShadowMap ? pcfShadow(pointShadowArray, pointShadowUv, pointShadowLayer, pointLightNdc.z - pointShadowBias) : 1.0; ///< Filter the selected point-face depth comparisons.
            const float pointShadowFactor = lerp(0.35, 1.0, pointShadowPcf); ///< Preserve the existing partial-shadow floor while smoothing its edge.
            pointContribution += pointLight * ((pointDiffuse + pointSpecular) * pointAttenuation * pointShadowFactor); ///< Add this point-light result with per-slot shadowing.
        }
    }
    const int activeDirectionalLightCount = min(max(int(frame.activeDirectionalLightCount), 0), kMaxDirectionalLights); ///< Clamp scene directional count to the fixed array.
    const float viewDepth = -mul(frame.view, float4(input.worldPos, 1.0)).z; ///< Camera looks down negative view-space Z, so visible depth is positive.
    int debugCascadeIndex = -1;                                      ///< Selected cascade retained only for the optional visualization.
    float3 directionalContribution = float3(0.0);                  ///< Accumulated directional-light response for all active directional lights.
    for (int i = 0; i < kMaxDirectionalLights; ++i) {              ///< Fixed trip count lets the compiler unroll the small directional loop.
        if (i < activeDirectionalLightCount) {                     ///< Ignore unused directional-light slots.
            const float3 dirSurfaceLightDir = normalize(-frame.directionalLightDirections[i].xyz); ///< Directional surface-to-light vector.
            const float dirDiffuse = max(dot(normal, dirSurfaceLightDir), 0.0); ///< Directional Lambertian light response.
            const float3 dirHalfVector = normalize(dirSurfaceLightDir + viewDir); ///< Directional Blinn-Phong halfway direction.
            const float dirSpecular = specularStrength * pow(max(dot(normal, dirHalfVector), 0.0), shininess); ///< Directional highlight response.
            const float3 dirLight = frame.directionalLightColorIntensities[i].rgb * frame.directionalLightColorIntensities[i].w; ///< Directional light color scaled by intensity.
            const float dirAmbient = frame.directionalLightAmbients[i].w; ///< Directional ambient contribution without shadowing.
            int dirCascade = -1;                                  ///< Negative means the fragment is beyond directional shadow coverage.
            for (int cascade = 0; cascade < kNumShadowCascades; ++cascade) {
                if (viewDepth <= frame.cascadeSplits[cascade]) { dirCascade = cascade; break; } ///< Select the nearest cascade whose far split contains the fragment.
            }
            float dirShadowPcf = 1.0;                              ///< Fragments beyond the last split remain fully lit.
            if (dirCascade >= 0) {
                const int dirShadowLayer = i * kNumShadowCascades + dirCascade; ///< Flattened light-cascade matrix and image layer.
                const float4 dirLightClip = mul(frame.dirLightViewProjArray[dirShadowLayer], float4(input.worldPos, 1.0)); ///< Fragment position in the selected cascade.
                const float3 dirLightNdc = dirLightClip.xyz / dirLightClip.w; ///< Directional light depth remains in 0..1 after divide.
                const float2 dirShadowUv = (dirLightNdc.xy * 0.5) + 0.5; ///< Convert directional light NDC XY to shadow texture UVs.
                const bool insideDirShadowMap = dirLightNdc.z >= 0.0 && dirLightNdc.z <= 1.0; ///< Reject fragments outside directional light depth coverage.
                const float dirShadowBias = 0.0005 * float(dirCascade + 1); ///< Approximate the increasing world texel size of farther cascades.
                dirShadowPcf = insideDirShadowMap ? pcfShadow(dirShadowArray, dirShadowUv, dirShadowLayer, dirLightNdc.z - dirShadowBias) : 1.0; ///< Filter the selected flattened cascade layer.
                debugCascadeIndex = dirCascade;
            }
            const float dirShadowFactor = lerp(0.35, 1.0, dirShadowPcf); ///< Preserve the existing partial-shadow floor while smoothing its edge.
            directionalContribution += dirAmbient + dirLight * ((dirDiffuse + dirSpecular) * dirShadowFactor); ///< Add this shadowed directional result.
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
            const bool insideSpotShadowMap = spotLightNdc.z >= 0.0 && spotLightNdc.z <= 1.0; ///< Reject fragments outside spot light depth coverage.
            const float spotShadowBias = perspectiveShadowBias(spotRange, spotLightClip.w); ///< Convert the world offset at this spot-axis distance.
            const float spotShadowPcf = insideSpotShadowMap ? pcfShadow(spotShadowArray, spotShadowUv, i, spotLightNdc.z - spotShadowBias) : 1.0; ///< Filter spotlight comparisons across neighboring texels.
            const float spotShadowFactor = lerp(0.35, 1.0, spotShadowPcf); ///< Preserve the existing partial-shadow floor while smoothing its edge.
            spotContribution += spotAmbient + spotLight * ((spotDiffuse + spotSpecular) * spotAttenuation * spotConeFalloff * spotShadowFactor); ///< Add this cone-attenuated spot result.
        }
    }
    float3 baseColor = input.color;                                      ///< Vertex color remains the default material color.
    if (object.useBaseColorTexture != 0) {
        baseColor *= baseColorTexture.Sample(input.texCoord).rgb;        ///< Optional texture modulates the material before lighting.
    }
    float3 litColor = baseColor * (ambient + pointContribution + directionalContribution + spotContribution); ///< Apply point, directional, and spot Blinn-Phong.
    if (kDebugCascadeTint && debugCascadeIndex >= 0) {
        const float3 cascadeTint = debugCascadeIndex == 0 ? float3(1.0, 0.25, 0.25) :
            (debugCascadeIndex == 1 ? float3(0.25, 1.0, 0.25) :
            (debugCascadeIndex == 2 ? float3(0.25, 0.25, 1.0) : float3(1.0, 1.0, 0.25))); ///< Red, green, blue, and yellow identify the four cascade regions.
        litColor = lerp(litColor, litColor * cascadeTint, 0.65); ///< Retain scene detail while making cascade boundaries obvious.
    }
    output.color = float4(litColor, 1.0);
    return output;
}
