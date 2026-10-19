#include "simple_shared.h"

/**
 * @file simple_forward.slang
 * @brief Vertex and fragment stages for the simple forward renderer.
 *
 * Vertex inputs: per-vertex position, normal, texture coordinate, and tangent attributes.
 * Vertex constants: shared camera and shadow matrices in set 0 binding 0, plus per-object push constants.
 * Shadow output: depth-only clip position from the shadow matrix selected by the push constant.
 * Fragment inputs: interpolated world-space position, texture coordinate, normal, and tangent from the vertex stage.
 * Fragment resources: the material texture table in binding 1, the spot, directional, and point shadow arrays in bindings 2-4,
 * and the dense material buffer in binding VVE_MATERIAL_BINDING.
 * Set-0 bindings mirror descriptorSetBindings in Vulkan/Pipeline.ixx; the material array is specialized to the device capacity.
 * Fragment outputs: one linear HDR color with alpha; the blended pipeline uses it for transparent objects.
 */

static const int kMaxShadowedSpotLights = VVE_MAX_SHADOWED_SPOT_LIGHTS; ///< Shared fixed spot-shadow cap.
static const int kMaxShadowedPointLights = VVE_MAX_SHADOWED_POINT_LIGHTS; ///< Shared fixed point-shadow cap.
static const int kMaxDirectionalLights = VVE_MAX_DIRECTIONAL_LIGHTS;  ///< Shared directional-light cap.
static const int kNumShadowCascades = VVE_NUM_SHADOW_CASCADES;        ///< Shared directional shadow cascade count.
static const int kShadowMapResolution = VVE_SHADOW_MAP_RESOLUTION;    ///< Shared shadow-map side length for software PCF offsets.
static const int kSpotShadowBase = 0;                                          ///< First spot matrix in shadowViewProjs (mirrors kShadowMatrixSpotBase).
static const int kPointShadowFaceCount = VVE_POINT_SHADOW_FACES; ///< Cubemap directions rendered per point light.
static const int kPointShadowBase = VVE_SHADOW_MATRIX_POINT_BASE;                    ///< First point-face matrix (mirrors kShadowMatrixPointBase).
static const int kDirShadowBase = VVE_SHADOW_MATRIX_DIR_BASE; ///< First directional cascade matrix (mirrors kShadowMatrixDirBase).
static const int kShadowMatrixCount = VVE_SHADOW_MATRIX_COUNT; ///< Mirrors kShadowMatrixCount.
static const float kDirectionalCompareBias = VVE_DIRECTIONAL_SHADOW_BIAS; ///< Receiver bias for the nearest directional cascade.
static const bool kDebugCascadeTint = false;  ///< Enables a visible red/green/blue/yellow directional-cascade overlay.

struct FrameUniforms
{
    float4x4 view;       ///< Camera view matrix shared by all objects in the frame.
    float4x4 projection; ///< Camera projection matrix shared by all objects in the frame.
    float4x4 shadowViewProjs[kShadowMatrixCount]; ///< Spot, point-face, and directional-cascade light matrices (see k*ShadowBase).
    float4 cascadeSplits; ///< View-space far distance of each directional shadow cascade.
    float4 pointLightPositionRanges[kMaxShadowedPointLights]; ///< Per-point xyz position with range in w.
    float4 pointLightColorIntensities[kMaxShadowedPointLights]; ///< Per-point rgb color with intensity in w.
    float4 spotLightPositionRanges[kMaxShadowedSpotLights]; ///< Per-spot xyz position with range in w.
    float4 spotLightColorIntensities[kMaxShadowedSpotLights]; ///< Per-spot rgb color with intensity in w.
    float4 spotLightDirections[kMaxShadowedSpotLights]; ///< Per-spot xyz light-to-scene direction; w = 1 skips shadows for ambient-only lights.
    float4 spotLightConeAmbients[kMaxShadowedSpotLights]; ///< Per-spot inner cone cosine, outer cone cosine, unused, ambient.
    float4 directionalLightDirections[kMaxDirectionalLights]; ///< Per-directional xyz light-to-scene direction; w = 1 skips shadows for ambient-only lights.
    float4 directionalLightColorIntensities[kMaxDirectionalLights]; ///< Per-directional rgb color with intensity in w.
    float4 directionalLightAmbients[kMaxDirectionalLights]; ///< Per-directional ambient term packed in w.
    uint activeDirectionalLightCount; ///< Packed directional-light count.
    uint activeSpotLightCount;        ///< Packed spot-light count.
    float ambient;                    ///< Scene-wide ambient term.
    uint activePointLightCount;       ///< Packed point-light count.
    float4 fogColorDensity;           ///< Linear haze RGB and extinction per world unit.
    float4 fogHeightTime;             ///< Base height, density height falloff, animation seconds, unused.
    float4x4 inverseViewProjection; ///< Inverse projection-view for depth reconstruction.
    float4 viewport; ///< Drawable dimensions.
    float4 environmentLighting;       ///< Panorama texture slot (-1 absent), strength, GGX enabled, unused.
    float4 skySun;                    ///< Unit direction towards the sun and its irradiance; w = 0 keeps the panorama and classic haze.
    float4 skyParams;                 ///< Aerosol amount, sun disk radius in radians, index of the sun's directional light (-1 none), unused.
    float4 cloudLayer;                ///< Cloud base height, thickness, coverage (0 none), extinction per metre at full density.
    float4 cloudWind;                 ///< Wind offset of the cloud pattern along x and z, unused, unused.
};

struct ObjectPushConstants
{
    float4x4 model;         ///< Object-local model matrix selected before each draw call.
    uint materialIndex;    ///< Dense slot in the material storage buffer.
    uint shadowMatrixIndex; ///< Index into frame.shadowViewProjs used by the shadow vertex stage.
    uint flags;             ///< Bit 0 unlit, bit 1 sky, bit 2 sprite, bit 3 water (mirrors objectFlag in Vulkan/Pipeline.ixx).
    float opacity;          ///< Object alpha multiplied with the material alpha.
    float4 waves;           ///< Water height, wavelength, angular speed and enabled flag.
};

struct GpuMaterial
{
    float4 baseColorFactor;         ///< Base-color factor with opacity in w; a base-color texture multiplies it.
    float4 emissiveFactor;          ///< Emissive rgb factor with the alpha mode in w (0 opaque, 1 cut-out, 2 blended).
    uint baseColorTexture;          ///< Base-color texture-table index.
    uint normalTexture;             ///< Normal texture-table index.
    uint metalnessTexture;          ///< Metalness texture-table index, sampled from blue (glTF packing; greyscale works too).
    uint roughnessTexture;          ///< Roughness texture-table index, sampled from green (glTF packing; greyscale works too).
    uint emissiveTexture;           ///< Emissive texture-table index.
    uint ambientOcclusionTexture;   ///< Ambient-occlusion texture-table index, sampled from red.
    float roughnessFactor;          ///< Roughness factor; a roughness texture multiplies it.
    float metalnessFactor;          ///< Metalness factor; a metalness texture multiplies it.
};

struct VertexInput
{
    [[vk::location(0)]] float3 position : POSITION;  ///< Object-local vertex position.
    [[vk::location(1)]] float3 normal   : NORMAL0;   ///< Object-local normal; zero means "use the face normal".
    [[vk::location(2)]] float2 texCoord : TEXCOORD0; ///< First texture coordinate, v pointing up in the image.
    [[vk::location(3)]] float4 tangent  : TANGENT0;  ///< Object-local tangent and handedness; zero when the mesh has none.
};

struct VertexOutput
{
    float4 position : SV_Position;                       ///< Clip-space position consumed by Vulkan.
    [[vk::location(0)]] float3 worldPos : TEXCOORD0;      ///< Interpolated world-space position for flat lighting.
    [[vk::location(1)]] float2 texCoord : TEXCOORD1;      ///< Interpolated texture coordinate for material sampling.
    [[vk::location(2)]] float3 normal   : NORMAL0;        ///< Interpolated world-space vertex normal.
    [[vk::location(3)]] float4 tangent  : TANGENT0;       ///< Interpolated world-space tangent and handedness.
};

struct FragmentInput
{
    float4 position : SV_Position;                  ///< Window position; dithers the cloud march of the sky.
    [[vk::location(0)]] float3 worldPos : TEXCOORD0; ///< Interpolated world-space position.
    [[vk::location(1)]] float2 texCoord : TEXCOORD1; ///< Interpolated texture coordinate for material sampling.
    [[vk::location(2)]] float3 normal   : NORMAL0;   ///< Interpolated world-space vertex normal.
    [[vk::location(3)]] float4 tangent  : TANGENT0;  ///< Interpolated world-space tangent and handedness.
};

struct FragmentOutput
{
    [[vk::location(0)]] float4 color : SV_Target0; ///< Color and alpha written to the HDR target.
};

[[vk::binding(0, 0)]]
ConstantBuffer<FrameUniforms> frame; ///< Frame-wide camera constants.

[[vk::constant_id(VVE_TEXTURE_CAPACITY_CONSTANT_ID)]]
const uint kMaxSceneTextures = 1; ///< Renderer specializes this array size to the selected device's descriptor capacity.
static const uint kNoTexture = VVE_NO_TEXTURE; ///< Absent material texture index defined in simple_shared.h.
static const uint kUnlitFlag = 1;  ///< ObjectPushConstants::flags bit for flat, unlit objects.
static const uint kSkyFlag = 2;    ///< ObjectPushConstants::flags bit for the skybox.
static const uint kSpriteFlag = 4; ///< ObjectPushConstants::flags bit for camera-facing quads.
static const uint kWaterFlag = 8; ///< Analytic waves with world-space height and normal derivatives.
static const float kPi = 3.14159265358979; ///< Converts sky angles to panorama coordinates.
static const float kAlphaMask = 1.0;  ///< Cut-out alpha mode stored in emissiveFactor.w.
static const float kAlphaBlend = 2.0; ///< Blended alpha mode stored in emissiveFactor.w.

[[vk::binding(1, 0)]]
Sampler2D baseColorTextures[kMaxSceneTextures]; ///< Shared material texture table selected by each material's texture indices.

[[vk::binding(2, 0)]]
Sampler2DArrayShadow spotShadowArray; ///< Spot light comparison-depth array, one layer per packed spot light.

[[vk::binding(3, 0)]]
Sampler2DArrayShadow dirShadowArray; ///< Directional light comparison-depth array, kNumShadowCascades layers per packed light.

[[vk::binding(4, 0)]]
Sampler2DArrayShadow pointShadowArray; ///< Point light comparison-depth array, six face layers per packed point light.

[[vk::binding(VVE_MATERIAL_BINDING, 0)]]
StructuredBuffer<GpuMaterial> materials; ///< Dense materials selected by each object's material index.

[[vk::push_constant]]
ObjectPushConstants object; ///< Per-object transform constants.

/**
 * @brief Converts a fixed world-space shadow offset to nonlinear Vulkan perspective depth.
 */
float perspectiveShadowBias(float farPlane, float axialDepth)
{
    const float nearPlane = VVE_SHADOW_NEAR_PLANE; ///< Mirrors the CPU perspective-shadow projection near plane.
    const float worldBias = VVE_PERSPECTIVE_SHADOW_WORLD_BIAS; ///< Keep perspective-shadow detachment bounded to two centimetres.
    const float biasedDepth = max(axialDepth, nearPlane + worldBias); ///< Guard the reciprocal near the light origin.
    const float depthScale = (farPlane * nearPlane) / max(farPlane - nearPlane, VVE_PERSPECTIVE_SHADOW_EPSILON); ///< Projection reciprocal coefficient.
    return max(depthScale * worldBias / (biasedDepth * max(biasedDepth - worldBias, VVE_PERSPECTIVE_SHADOW_EPSILON)), VVE_PERSPECTIVE_SHADOW_MIN_BIAS);
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

/**
 * @brief Normalizes a vector, but returns zero for a zero vector instead of NaN.
 */
float3 normalizeOrZero(float3 v)
{
    const float lengthSquared = dot(v, v);
    return lengthSquared > 1.0e-12 ? v * rsqrt(lengthSquared) : float3(0.0);
}

/**
 * @brief Normalizes a transformed mesh direction; zero when the mesh supplies none.
 *
 * The test uses the untransformed vector, so tiny object scales (millimetre models) do not drop valid normals.
 */
float3 normalizeTransformed(float3 source, float3 transformed)
{
    const float lengthSquared = dot(transformed, transformed);
    return dot(source, source) > 0.0 && lengthSquared > 0.0 ? transformed * rsqrt(lengthSquared) : float3(0.0);
}

/**
 * @brief Returns the matrix that transforms normals: the cofactor of the model's upper 3x3.
 *
 * The cofactor is the inverse transpose scaled by the determinant, so normals stay perpendicular to the
 * surface under non-uniform scale. Its sign is corrected so that mirroring transforms keep outward normals.
 */
float3x3 normalMatrix(float4x4 model)
{
    const float3x3 m = float3x3(model);
    const float3x3 cofactor = float3x3(cross(m[1], m[2]), cross(m[2], m[0]), cross(m[0], m[1]));
    const float determinant = dot(m[0], cofactor[0]);
    return determinant < 0.0 ? -cofactor : cofactor;
}

/**
 * @brief Half a texel of one table texture; clamping by it keeps the repeat sampler from wrapping over image edges.
 */
float2 halfTexel(uint texture)
{
    uint width, height;
    baseColorTextures[texture].GetDimensions(width, height);
    return 0.5 / float2(width, height);
}

/// @brief World eye from the inverse rigid view; reused by lighting and height-dependent haze.
float3 worldEye()
{
    return -mul(transpose(float3x3(frame.view)), float3(frame.view[0][3], frame.view[1][3], frame.view[2][3]));
}

/// @brief Two directional sine waves return height and horizontal derivatives for matching surface normals.
float3 oceanWave(float2 p)
{
    const float k = 2.0 * kPi / max(object.waves.y, 0.001);
    const float t = object.waves.z * frame.fogHeightTime.z;
    const float a = k * (p.x + 0.37 * p.y) - t;
    const float b = 1.31 * k * (0.33 * p.x + p.y) - 0.73 * t;
    return object.waves.x * float3(0.65 * sin(a) + 0.35 * sin(b),
        k * (0.65 * cos(a) + 0.35 * 1.31 * 0.33 * cos(b)), k * (0.65 * 0.37 * cos(a) + 0.35 * 1.31 * cos(b)));
}

#include "simple_atmosphere.slang" // Physical sky, clouds and weather haze.
#include "simple_material.slang" // Shared direct BRDF and bounded-cost panorama filtering.

/// @brief Fades a surface by the physical sky, or by exponential height haze; zero density leaves old scenes unchanged.
float3 applyAtmosphere(float3 color, float3 worldPos)
{
    if (physicalSky()) return applySkyAtmosphere(color, worldPos);
    if (frame.fogColorDensity.w == 0.0) return color;
    const float3 eye = worldEye();
    const float d = length(worldPos - eye);
    return d > 0.0 ? lerp(frame.fogColorDensity.rgb, color, exp(-hazeDepth(eye, (worldPos - eye) / d, d))) : color;
}

[shader("vertex")]
VertexOutput vertexMain(VertexInput input)
{
    VertexOutput output;
    // The sky follows the camera's rotation only and lies just inside the far plane; the fragment stage gets its direction.
    if ((object.flags & kSkyFlag) != 0) {
        output.position = mul(frame.projection, float4(mul(float3x3(frame.view), input.position), 1.0));
        output.position.z = 0.999999 * output.position.w;
        output.worldPos = input.position;                                ///< Object-space direction from the cube centre.
        output.texCoord = float2(0.0);
        output.normal = float3(0.0);
        output.tangent = float4(0.0);
        return output;
    }
    float4 worldPosition = mul(object.model, float4(input.position, 1.0));
    if ((object.flags & kWaterFlag) != 0) worldPosition.y += oceanWave(worldPosition.xz).x;
    // Sprites keep their world origin and span the quad along the camera's right and up vectors (view rows 0 and 1).
    if ((object.flags & kSpriteFlag) != 0) {
        const float2 scale = float2(length(mul(object.model, float4(1.0, 0.0, 0.0, 0.0)).xyz), length(mul(object.model, float4(0.0, 1.0, 0.0, 0.0)).xyz));
        worldPosition = mul(object.model, float4(0.0, 0.0, 0.0, 1.0));
        worldPosition.xyz += frame.view[0].xyz * (input.position.x * scale.x) + frame.view[1].xyz * (input.position.y * scale.y);
        output.position = mul(frame.projection, mul(frame.view, worldPosition));
        output.worldPos = worldPosition.xyz;
        output.texCoord = input.texCoord;
        output.normal = frame.view[2].xyz;                               ///< Points towards the camera.
        output.tangent = float4(0.0);
        return output;
    }
    const float4 viewPosition = mul(frame.view, worldPosition);
    output.position = mul(frame.projection, viewPosition);
    output.worldPos = worldPosition.xyz;
    output.texCoord = input.texCoord; ///< Preserve vertex UVs for the fragment interface.
    output.normal = normalizeTransformed(input.normal, mul(normalMatrix(object.model), input.normal)); ///< Stays zero for meshes without normals.
    output.tangent = float4(normalizeTransformed(input.tangent.xyz, mul(float3x3(object.model), input.tangent.xyz)), input.tangent.w); ///< Tangents follow the surface, so the model matrix applies.
    return output;
}

[shader("vertex")]
float4 shadowVertexMain(VertexInput input) : SV_Position
{
    float4 worldPosition = mul(object.model, float4(input.position, 1.0));
    if ((object.flags & kWaterFlag) != 0) worldPosition.y += oceanWave(worldPosition.xz).x;
    return mul(frame.shadowViewProjs[object.shadowMatrixIndex], worldPosition); ///< Shadows follow the same animated surface.
}

/// @brief Three material attachments, evaluated once by either geometry path in linear colour space.
struct GBufferOutput {
    [[vk::location(0)]] float4 baseRoughness : SV_Target0; ///< Linear HDR base colour and roughness.
    [[vk::location(1)]] float4 normalMetalness : SV_Target1; ///< World-space unit normal and metalness.
    [[vk::location(2)]] float4 emissiveFlags : SV_Target2; ///< HDR emission; AO + twice the unlit/water flags.
};

/// @brief Samples material maps and applies cut-out alpha, normal transforms and animated water for both backends.
GBufferOutput evaluateSurface(FragmentInput input, out float alpha)
{
    const GpuMaterial material = materials[object.materialIndex];
    float3 baseColor = material.baseColorFactor.rgb;                      ///< Material factor remains the default base color.
    float coverage = material.baseColorFactor.a;                         ///< Material alpha; the texture alpha multiplies it.
    if (material.baseColorTexture != kNoTexture && material.baseColorTexture < kMaxSceneTextures) {
        float2 texCoord = input.texCoord;
        if ((object.flags & kSpriteFlag) != 0) {                          ///< A sprite edge must not blend with the opposite edge.
            const float2 edge = halfTexel(material.baseColorTexture);
            texCoord = clamp(texCoord, edge, 1.0 - edge);
        }
        const float4 texel = baseColorTextures[material.baseColorTexture].Sample(texCoord);
        baseColor *= texel.rgb;                                          ///< The material texture modulates its factor before lighting.
        coverage *= texel.a;
    }
    if (material.emissiveFactor.w == kAlphaMask && coverage < 0.5) { discard; } ///< Cut-out leaves holes, the rest is opaque.
    const bool blended = material.emissiveFactor.w == kAlphaBlend || (object.flags & kSpriteFlag) != 0; ///< Sprites always use their texture alpha.
    alpha = (blended ? coverage : 1.0) * object.opacity;    ///< Opaque modes ignore material alpha.
    float roughness = material.roughnessFactor;                          ///< Factors apply alone or scale their texture, as in glTF.
    if (material.roughnessTexture < kMaxSceneTextures) {
        roughness *= baseColorTextures[material.roughnessTexture].Sample(input.texCoord).g; ///< glTF packs roughness in green.
    }
    roughness = saturate(roughness);
    float metalness = material.metalnessFactor;
    if (material.metalnessTexture < kMaxSceneTextures) {
        metalness *= baseColorTextures[material.metalnessTexture].Sample(input.texCoord).b; ///< glTF packs metalness in blue.
    }
    metalness = saturate(metalness);
    float3 emissive = material.emissiveFactor.rgb;
    if (material.emissiveTexture < kMaxSceneTextures) {
        emissive *= baseColorTextures[material.emissiveTexture].Sample(input.texCoord).rgb;
    }
    const float ambientOcclusion = material.ambientOcclusionTexture < kMaxSceneTextures ?
        baseColorTextures[material.ambientOcclusionTexture].Sample(input.texCoord).r : 1.0;
    const float3 rawNormal = normalize(cross(ddx(input.worldPos), ddy(input.worldPos))); ///< Flat geometric face normal from screen derivatives.
    const float3 cameraPos = worldEye(); ///< One inverse view calculation shared with the atmosphere.
    const float3 viewDir = normalize(cameraPos - input.worldPos);        ///< Surface-to-eye vector for view-dependent highlights.
    float3 normal = length(input.normal) > 0.5 ? normalize(input.normal) : rawNormal; ///< Prefer vertex normals when the mesh has them.
    if (material.normalTexture < kMaxSceneTextures) {
        const float3 tangentRaw = input.tangent.xyz - normal * dot(normal, input.tangent.xyz); ///< Gram-Schmidt tangent orthogonalization.
        const float tangentLengthSquared = dot(tangentRaw, tangentRaw);
        if (tangentLengthSquared > 1.0e-8) {                             ///< Without a usable tangent the geometric normal is kept.
            const float3 tangent = tangentRaw * rsqrt(tangentLengthSquared);
            const float3 bitangent = cross(normal, tangent) * (input.tangent.w < 0.0 ? -1.0 : 1.0); ///< Handedness preserves mirrored UV orientation.
            const float3 mappedNormal = baseColorTextures[material.normalTexture].Sample(input.texCoord).rgb * 2.0 - 1.0;
            const float3 perturbed = normalizeOrZero(tangent * mappedNormal.x + bitangent * mappedNormal.y + normal * mappedNormal.z);
            normal = dot(perturbed, perturbed) > 0.0 ? perturbed : normal;
        }
    }
    if ((object.flags & kWaterFlag) != 0) {
        const float3 wave = oceanWave(input.worldPos.xz);
        normal = normalize(float3(normal.x - wave.y * normal.y, normal.y, normal.z - wave.z * normal.y)); ///< Transform the base normal by the wave displacement.
    }
    normal = dot(normal, viewDir) < 0.0 ? -normal : normal; ///< Keep visible debug faces lit even when winding flips normals.
    GBufferOutput result;
    result.baseRoughness = float4(baseColor, roughness);
    result.normalMetalness = float4(normal, metalness);
    result.emissiveFlags = float4(emissive, ambientOcclusion + 2.0 * float(object.flags & (kUnlitFlag | kWaterFlag)));
    return result;
}
/// @brief One lighting implementation shared by forward fragments and reconstructed deferred samples.
float3 shadeSurface(float3 worldPos, GBufferOutput surface, uint flags)
{
    const float3 baseColor = surface.baseRoughness.rgb;
    if ((flags & kUnlitFlag) != 0) return applyAtmosphere(baseColor, worldPos);
    const float roughness = surface.baseRoughness.a;
    const float metalness = surface.normalMetalness.a;
    const float3 normal = normalize(surface.normalMetalness.xyz);
    const float3 emissive = surface.emissiveFlags.rgb;
    const float ambientOcclusion = surface.emissiveFlags.a - 2.0 * float(flags);
    const float3 viewDir = normalize(worldEye() - worldPos);
    const float ambient = frame.ambient * ambientOcclusion;              ///< Ambient occlusion attenuates indirect scene light.
    const float shininess = lerp(4.0, 128.0, (1.0 - roughness) * (1.0 - roughness)); ///< Rough surfaces broaden Blinn-Phong highlights.
    const float specularStrength = lerp(0.3, 1.0, metalness);            ///< Metals strengthen their tinted reflection.
    const float3 diffuseColor = baseColor * (1.0 - metalness);           ///< Metals have no diffuse response.
    const float3 specularColor = lerp(float3(specularStrength), baseColor * specularStrength, metalness); ///< Metallic highlights use base color.
    const bool physicallyBased = frame.environmentLighting.z != 0.0 || (flags & kWaterFlag) != 0;
    float3 pointContribution = float3(0.0);                              ///< Accumulated point-light response for active point slots.
    // The CPU packs contributing point lights densely and uploads their count.
    for (uint i = 0; i < frame.activePointLightCount; ++i) {
        const float pointRangeRaw = frame.pointLightPositionRanges[i].w; ///< Range of this packed point light.
        const float pointIntensity = frame.pointLightColorIntensities[i].w; ///< Positive intensity selected during CPU packing.
        const float3 pointOffset = frame.pointLightPositionRanges[i].xyz - worldPos; ///< Vector from fragment to this point light.
        const float pointDistance = length(pointOffset);             ///< Fragment-light distance for falloff.
        const float3 pointLightDir = pointOffset / max(pointDistance, 0.001); ///< Point-light direction changes across the scene.
        const float pointRangeFalloff = saturate(1.0 - (pointDistance / pointRangeRaw)); ///< Fade direct light at the configured range.
        const float pointAttenuation = pointRangeFalloff * pointRangeFalloff; ///< Smooth enough quadratic falloff for the debug scene.
        const float pointDiffuse = max(dot(normal, pointLightDir), 0.0); ///< Lambertian response now varies with point-light position.
        const float3 pointHalfVector = normalize(pointLightDir + viewDir); ///< Blinn-Phong halfway direction between light and eye.
        const float pointSpecular = pow(max(dot(normal, pointHalfVector), 0.0), shininess); ///< Point-light highlight response.
        const float3 pointLight = frame.pointLightColorIntensities[i].rgb * pointIntensity; ///< Point light color scaled by intensity.
        const float3 pointLightToFragment = worldPos - frame.pointLightPositionRanges[i].xyz; ///< Vector selects the cubemap-style shadow face.
        const float3 pointFaceAbs = abs(pointLightToFragment); ///< Dominant axis chooses the CPU face order.
        int pointFace = pointFaceAbs.x >= pointFaceAbs.y && pointFaceAbs.x >= pointFaceAbs.z ? (pointLightToFragment.x >= 0.0 ? 0 : 1) :
            (pointFaceAbs.y >= pointFaceAbs.z ? (pointLightToFragment.y >= 0.0 ? 2 : 3) : (pointLightToFragment.z >= 0.0 ? 4 : 5)); ///< Face order: +X, -X, +Y, -Y, +Z, -Z.
        const int pointShadowLayer = i * kPointShadowFaceCount + pointFace; ///< Dense depth-array layer for this point-light face.
        const float4 pointLightClip = mul(frame.shadowViewProjs[kPointShadowBase + pointShadowLayer], float4(worldPos, 1.0)); ///< Fragment in selected point-face clip space.
        const float pointClipW = abs(pointLightClip.w) > 0.001 ? pointLightClip.w : 0.001; ///< Avoid invalid perspective division at the light origin.
        const float3 pointLightNdc = pointLightClip.xyz / pointClipW; ///< Point face depth remains in 0..1 after divide.
        const float2 pointShadowUv = (pointLightNdc.xy * 0.5) + 0.5; ///< Convert point face NDC XY to shadow texture UVs.
        const bool insidePointShadowMap = pointLightClip.w > 0.001 && pointShadowUv.x >= 0.0 && pointShadowUv.x <= 1.0 &&
            pointShadowUv.y >= 0.0 && pointShadowUv.y <= 1.0 && pointLightNdc.z >= 0.0 && pointLightNdc.z <= 1.0; ///< Reject fragments outside the selected point face.
        const float pointShadowBias = perspectiveShadowBias(pointRangeRaw, pointLightClip.w); ///< Convert the world offset at this face-axis distance.
        const float pointShadowPcf = insidePointShadowMap ? pcfShadow(pointShadowArray, pointShadowUv, pointShadowLayer, pointLightNdc.z - pointShadowBias) : 1.0; ///< Filter the selected point-face depth comparisons.
        const float pointShadowFactor = lerp(VVE_SHADOW_OCCLUDED_FACTOR, 1.0, pointShadowPcf); ///< Preserve the existing partial-shadow floor while smoothing its edge.
        const float3 pointResponse = physicallyBased ? microfacetLight(baseColor, metalness, roughness, normal, viewDir, pointLightDir, flags) : diffuseColor * pointDiffuse + specularColor * pointSpecular;
        pointContribution += pointLight * pointResponse * pointAttenuation * pointShadowFactor; ///< Visibility and existing range falloff apply to either material model.
    }
    const int activeDirectionalLightCount = min(max(int(frame.activeDirectionalLightCount), 0), kMaxDirectionalLights); ///< Clamp scene directional count to the fixed array.
    const float viewDepth = -mul(frame.view, float4(worldPos, 1.0)).z; ///< Camera looks down negative view-space Z, so visible depth is positive.
    int debugCascadeIndex = -1;                                      ///< Selected cascade retained only for the optional visualization.
    float3 directionalContribution = float3(0.0);                  ///< Accumulated directional-light response for all active directional lights.
    for (int i = 0; i < kMaxDirectionalLights; ++i) {              ///< Fixed trip count lets the compiler unroll the small directional loop.
        if (i < activeDirectionalLightCount) {                     ///< Ignore unused directional-light slots.
            // This per-light uniform branch preserves ambient without evaluating shadow matrices or PCF.
            if (frame.directionalLightDirections[i].w != 0.0) {
                directionalContribution += baseColor * frame.directionalLightAmbients[i].w * ambientOcclusion;
                continue;
            }
            const float3 dirSurfaceLightDir = -frame.directionalLightDirections[i].xyz; ///< CPU-normalized directional surface-to-light vector.
            const float dirDiffuse = max(dot(normal, dirSurfaceLightDir), 0.0); ///< Directional Lambertian light response.
            const float3 dirHalfVector = normalize(dirSurfaceLightDir + viewDir); ///< Directional Blinn-Phong halfway direction.
            const float dirSpecular = pow(max(dot(normal, dirHalfVector), 0.0), shininess); ///< Directional highlight response.
            const float3 dirLight = frame.directionalLightColorIntensities[i].rgb * frame.directionalLightColorIntensities[i].w; ///< Directional light color scaled by intensity.
            const float dirAmbient = frame.directionalLightAmbients[i].w * ambientOcclusion; ///< Directional ambient contribution without shadowing.
            int dirCascade = -1;                                  ///< Negative means the fragment is beyond directional shadow coverage.
            for (int cascade = 0; cascade < kNumShadowCascades; ++cascade) {
                if (viewDepth <= frame.cascadeSplits[cascade]) { dirCascade = cascade; break; } ///< Select the nearest cascade whose far split contains the fragment.
            }
            float dirShadowPcf = 1.0;                              ///< Fragments beyond the last split remain fully lit.
            if (dirCascade >= 0) {
                const int dirShadowLayer = i * kNumShadowCascades + dirCascade; ///< Flattened light-cascade matrix and image layer.
                const float4 dirLightClip = mul(frame.shadowViewProjs[kDirShadowBase + dirShadowLayer], float4(worldPos, 1.0)); ///< Fragment position in the selected cascade.
                const float3 dirLightNdc = dirLightClip.xyz / dirLightClip.w; ///< Directional light depth remains in 0..1 after divide.
                const float2 dirShadowUv = (dirLightNdc.xy * 0.5) + 0.5; ///< Convert directional light NDC XY to shadow texture UVs.
                const bool insideDirShadowMap = dirLightNdc.z >= 0.0 && dirLightNdc.z <= 1.0; ///< Reject fragments outside directional light depth coverage.
                const float dirShadowBias = kDirectionalCompareBias * float(dirCascade + 1); ///< Small receiver offset avoids detaching contact shadows while raster bias prevents acne.
                dirShadowPcf = insideDirShadowMap ? pcfShadow(dirShadowArray, dirShadowUv, dirShadowLayer, dirLightNdc.z - dirShadowBias) : 1.0; ///< Filter the selected flattened cascade layer.
                debugCascadeIndex = dirCascade;
            }
            const float dirShadowFactor = lerp(VVE_SHADOW_OCCLUDED_FACTOR, 1.0, dirShadowPcf) *
                (float(i) == frame.skyParams.z ? cloudShadow(worldPos) : 1.0); ///< Shadow floor and smooth edge; clouds shade the sun.
            const float3 dirResponse = physicallyBased ? microfacetLight(baseColor, metalness, roughness, normal, viewDir, dirSurfaceLightDir, flags) : diffuseColor * dirDiffuse + specularColor * dirSpecular;
            directionalContribution += baseColor * dirAmbient + dirLight * dirResponse * dirShadowFactor; ///< Ambient and shadows retain their established controls.
        }
    }
    const int activeSpotLightCount = min(max(int(frame.activeSpotLightCount), 0), kMaxShadowedSpotLights); ///< Clamp scene spot count to the fixed shadow array.
    float3 spotContribution = float3(0.0);                           ///< Accumulated spot-light response for all active shadowed spots.
    for (int i = 0; i < kMaxShadowedSpotLights; ++i) {                ///< Fixed trip count lets the compiler unroll the small spot loop.
        if (i < activeSpotLightCount) {                               ///< Ignore unused shadow-array layers.
            // Ambient-only slots have no shadow image contents to sample.
            if (frame.spotLightDirections[i].w != 0.0) {
                spotContribution += baseColor * frame.spotLightConeAmbients[i].w * ambientOcclusion;
                continue;
            }
            const float3 spotToLight = frame.spotLightPositionRanges[i].xyz - worldPos; ///< Vector from fragment to the spot light.
            const float spotDistance = length(spotToLight);           ///< Fragment-spot distance for falloff.
            const float3 spotLightDir = spotToLight / max(spotDistance, 0.001); ///< Spot surface-to-light vector for direct response.
            const float spotRange = max(frame.spotLightPositionRanges[i].w, 0.001); ///< Positive spot range for stable attenuation.
            const float spotRangeValid = frame.spotLightPositionRanges[i].w > 0.0 ? 1.0 : 0.0; ///< Disable spot light for invalid ranges.
            const float spotRangeFalloff = saturate(1.0 - (spotDistance / spotRange)) * spotRangeValid; ///< Fade spot light at the configured range.
            const float spotAttenuation = spotRangeFalloff * spotRangeFalloff; ///< Spot quadratic distance falloff.
            const float spotDiffuse = max(dot(normal, spotLightDir), 0.0); ///< Spot Lambertian light response.
            const float3 spotHalfVector = normalize(spotLightDir + viewDir); ///< Spot Blinn-Phong halfway direction.
            const float spotSpecular = pow(max(dot(normal, spotHalfVector), 0.0), shininess); ///< Spot highlight response.
            const float spotCos = dot(-spotLightDir, frame.spotLightDirections[i].xyz); ///< Cone cosine using the CPU-normalized spot axis.
            const float spotConeFalloff = smoothstep(frame.spotLightConeAmbients[i].y, frame.spotLightConeAmbients[i].x, spotCos); ///< Spot cone angular falloff.
            const float3 spotLight = frame.spotLightColorIntensities[i].rgb * frame.spotLightColorIntensities[i].w; ///< Spot light color scaled by intensity.
            const float spotAmbient = frame.spotLightConeAmbients[i].w * ambientOcclusion; ///< Spot ambient contribution without shadowing.
            const float4 spotLightClip = mul(frame.shadowViewProjs[kSpotShadowBase + i], float4(worldPos, 1.0)); ///< Fragment position in spot light clip space.
            const float3 spotLightNdc = spotLightClip.xyz / spotLightClip.w; ///< Spot light depth remains in 0..1 after divide.
            const float2 spotShadowUv = (spotLightNdc.xy * 0.5) + 0.5; ///< Convert spot light NDC XY to shadow texture UVs.
            const bool insideSpotShadowMap = spotLightNdc.z >= 0.0 && spotLightNdc.z <= 1.0; ///< Reject fragments outside spot light depth coverage.
            const float spotShadowBias = perspectiveShadowBias(spotRange, spotLightClip.w); ///< Convert the world offset at this spot-axis distance.
            const float spotShadowPcf = insideSpotShadowMap ? pcfShadow(spotShadowArray, spotShadowUv, i, spotLightNdc.z - spotShadowBias) : 1.0; ///< Filter spotlight comparisons across neighboring texels.
            const float spotShadowFactor = lerp(VVE_SHADOW_OCCLUDED_FACTOR, 1.0, spotShadowPcf); ///< Preserve the existing partial-shadow floor while smoothing its edge.
            const float3 spotResponse = physicallyBased ? microfacetLight(baseColor, metalness, roughness, normal, viewDir, spotLightDir, flags) : diffuseColor * spotDiffuse + specularColor * spotSpecular;
            spotContribution += baseColor * spotAmbient + spotLight * spotResponse * spotAttenuation * spotConeFalloff * spotShadowFactor;
        }
    }
    float3 litColor = baseColor * ambient + pointContribution + directionalContribution + spotContribution + emissive; ///< Emissive remains visible independently of shadows.
    if (physicallyBased) {
        const float3 fresnel = fresnelSchlick(saturate(dot(normal, viewDir)), lerp(float3((flags & kWaterFlag) != 0 ? 0.02 : 0.04), baseColor, metalness));
        litColor += ambientOcclusion * ((1.0 - fresnel) * diffuseColor * sampleEnvironment(normal, 1.0) +
            fresnel * sampleEnvironment(reflect(-viewDir, normal), roughness)); ///< Sky illumination is independent of authored ambient lights.
    }
    if (kDebugCascadeTint && debugCascadeIndex >= 0) {
        const float3 cascadeTint = debugCascadeIndex == 0 ? float3(1.0, 0.25, 0.25) :
            (debugCascadeIndex == 1 ? float3(0.25, 1.0, 0.25) :
            (debugCascadeIndex == 2 ? float3(0.25, 0.25, 1.0) : float3(1.0, 1.0, 0.25))); ///< Red, green, blue, and yellow identify the four cascade regions.
        litColor = lerp(litColor, litColor * cascadeTint, 0.65); ///< Retain scene detail while making cascade boundaries obvious.
    }
    return applyAtmosphere(litColor, worldPos);
}

[shader("fragment")]
FragmentOutput fragmentMain(FragmentInput input)
{
    FragmentOutput output;
    const GpuMaterial material = materials[object.materialIndex];
    // Equirectangular lookup; textures are stored bottom-up, so v = 1 is the image's top row (straight up).
    if ((object.flags & kSkyFlag) != 0) {
        const float3 d = normalize(input.worldPos);
        if (physicalSky()) {
            output.color = float4(skyColour(d, frac(52.9829189 * frac(dot(input.position.xy, float2(0.06711056, 0.00583715))))), 1.0);
            return output;
        }
        float2 uv = float2(atan2(d.x, -d.z) / (2.0 * kPi) + 0.5, 1.0 - acos(clamp(d.y, -1.0, 1.0)) / kPi);
        float3 sky = float3(1.0);
        if (material.baseColorTexture < kMaxSceneTextures) {
            const float edge = halfTexel(material.baseColorTexture).y;
            uv.y = clamp(uv.y, edge, 1.0 - edge);                        ///< The repeat sampler must not wrap over the poles.
            sky = baseColorTextures[material.baseColorTexture].SampleLevel(uv, 0.0).rgb; ///< Level 0 avoids a seam where u wraps.
        }
        output.color = float4(sky * material.baseColorFactor.rgb, 1.0);
        return output;
    }
    float alpha;
    const GBufferOutput surface = evaluateSurface(input, alpha);
    output.color = float4(shadeSurface(input.worldPos, surface, object.flags & (kUnlitFlag | kWaterFlag)), alpha);
    return output;
}

/// @brief Opaque geometry writes attributes without evaluating any lights; cut-out still uses the shared material function.
[shader("fragment")]
GBufferOutput gbufferMain(FragmentInput input)
{
    float alpha;
    return evaluateSurface(input, alpha);
}

/// @brief Interpolates every material input at the same coverage sample whose depth will reconstruct its position.
[shader("fragment")]
GBufferOutput gbufferMsaaMain(FragmentInput input, uint sampleIndex : SV_SampleIndex)
{
    input.worldPos = EvaluateAttributeAtSample(input.worldPos, sampleIndex);
    input.texCoord = EvaluateAttributeAtSample(input.texCoord, sampleIndex);
    input.normal = EvaluateAttributeAtSample(input.normal, sampleIndex);
    input.tangent = EvaluateAttributeAtSample(input.tangent, sampleIndex);
    float alpha;
    return evaluateSurface(input, alpha);
}
