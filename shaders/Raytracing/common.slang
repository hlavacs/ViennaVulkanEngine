// Shared declarations for the basic VERendererRaytracing shaders.
//
// NOTE FOR AI AGENTS:
// Unlike shaders/Raytracing/rtbasic.slang (taken from the Nvidia tutorial),
// these shaders are written specifically for this project. They only rely on
// the descriptor set layout created in RendererRaytracing::CreateRayTracingPipeline:
//   binding 0 -> top level acceleration structure (TLAS)
//   binding 1 -> RWTexture2D<float4> output / storage image
//   binding 2 -> camera uniform buffer (inverse view / projection matrices)
// There are no push constants and no external includes.

// Camera data, must match the CameraRT struct in VERendererRaytracing.cpp.
struct CameraRT
{
    float4x4 viewInverse;
    float4x4 projInverse;
};

// Ray payload carried through the ray tracing pipeline.
struct HitPayload
{
    float3 color;
};

// --- Descriptor bindings (set 0) ---
[[vk::binding(0, 0)]] RaytracingAccelerationStructure topLevelAS;
[[vk::binding(1, 0)]] RWTexture2D<float4> outImage;
[[vk::binding(2, 0)]] ConstantBuffer<CameraRT> camera;
