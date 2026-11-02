/** @file @brief Test-owned box geometry and material stages, using only the public native shader contract. */
struct Constants {
    float4x4 clip; ///< Pass view-projection, including Vulkan Y flip.
    float4 low, high; ///< World box corners; w carries probe strength/enabled for forward sampling.
    float4 color; ///< Linear reflectance and forward opacity.
    float4 eye; ///< Capture eye/range, or main probe eye/cascade count.
};
[[vk::push_constant]] Constants object;
#define VVE_IRRADIANCE_EYE object.eye.xyz
#define VVE_IRRADIANCE_ORIGIN float4((floor(object.eye.xyz / VVE_PROBE_SPACING) - VVE_PROBE_SIDE / 2) * VVE_PROBE_SPACING, VVE_PROBE_SPACING)
#define VVE_IRRADIANCE_CASCADES object.eye.w
#define VVE_IRRADIANCE_VISIBILITY 1
#include "vve_irradiance.slang"
/// @brief Interpolated world geometry; flat box normals do not depend on the camera.
struct Vertex { float4 position : SV_Position; float3 world : TEXCOORD0; float3 normal : TEXCOORD1; };
/// @brief Generate outward-wound box faces without engine meshes, buffers, material descriptors or private shaders.
[shader("vertex")]
Vertex vertexMain(uint vertex : SV_VulkanVertexID) {
    const uint corners[36]={1,3,7,1,7,5, 0,4,6,0,6,2, 2,6,7,2,7,3, 0,1,5,0,5,4, 4,5,7,4,7,6, 0,2,3,0,3,1};
    const float3 normals[6]={float3(1,0,0),float3(-1,0,0),float3(0,1,0),float3(0,-1,0),float3(0,0,1),float3(0,0,-1)};
    const uint c=corners[vertex];
    Vertex result;
    result.world=lerp(object.low.xyz,object.high.xyz,float3(c&1,(c>>1)&1,(c>>2)&1));
    result.normal=normals[vertex/6]; result.position=mul(object.clip,float4(result.world,1));
    return result;
}
/// @brief The three public material attachments: linear albedo/roughness, signed world normal/metalness, emission/AO+2*flags.
struct GBuffer { float4 base : SV_Target0; float4 normal : SV_Target1; float4 emission : SV_Target2; };
[shader("fragment")]
GBuffer gbufferMain(Vertex input) {
    GBuffer result;
    result.base=float4(object.color.rgb,.5); result.normal=float4(input.normal,0); result.emission=float4(0,0,0,1);
    return result;
}
/// @brief Direct-only diffuse radiance; the neutral floor has exactly zero cosine to this horizontal sun.
[shader("fragment")]
float4 captureMain(Vertex input,bool front : SV_IsFrontFace) : SV_Target0 {
    const float3 direct=object.color.rgb*4*max(input.normal.x,0);
    return float4(direct,min(length(input.world-object.eye.xyz),object.eye.w)*(front ? 1 : -1));
}
/// @brief Old backends require no probe bindings; used also to exercise their forward-opaque hook.
[shader("fragment")]
float4 plainMain(Vertex input) : SV_Target0 { return object.color; }
/// @brief Indirect-only diagnostic: no authored ambient, direct, emission, tone map or AO can supply its chromatic signal.
[shader("fragment")]
float4 forwardMain(Vertex input) : SV_Target0 {
    float coverage; float3 incoming=float3(0);
    if (object.high.w!=0 && object.low.w>0) incoming=sampleProbeIrradiance(input.world,input.normal,coverage)*object.low.w;
    return float4(object.color.rgb*incoming,object.color.a);
}
