// Shared declarations for the VERendererRaytracing Whitted ray tracer.
//
// NOTE FOR AI AGENTS:
// These shaders are written specifically for this project. They rely on the
// descriptor set layout created in RendererRaytracing::CreateRayTracingPipeline:
//   binding 0 -> top level acceleration structure (TLAS)
//   binding 1 -> RWTexture2D<float4> output / storage image
//   binding 2 -> camera uniform buffer (inverse view / projection, position, light counts)
//   binding 3 -> StructuredBuffer<InstanceData> (per TLAS instance material + geometry)
//   binding 4 -> StructuredBuffer<Light> (scene lights)
//   binding 5 -> Sampler2D textures[] (bindless texture array)
// Per-instance vertex / index data is reached through buffer-device-address
// pointers stored inside InstanceData (buffer_reference style, like the Nvidia
// tutorial). There are no push constants.

// Maximum recursion used for reflection rays (kept small on purpose).
static const uint MAX_RAY_DEPTH = 4;

// Camera data, must match the CameraRT struct in VERendererRaytracing.cpp.
struct CameraRT
{
    float4x4 viewInverse;
    float4x4 projInverse;
    float4   cameraPos;  // xyz = world space camera position
    int4     numLights;  // x=point, y=directional, z=spot, w=total
};

// Per TLAS instance data. Must match InstanceDataGPU in VERendererRaytracing.cpp
// (std430 layout). Vertex/index buffers are physical-storage-buffer pointers.
struct InstanceData
{
    float4    ambient;
    float4    diffuse;
    float4    specular;
    float*    vertexBuffer; // interleaved attribute blocks (float32)
    uint*     indexBuffer;
    float2    uvScale;
    int       textureIndex;
    uint      normalOffset;
    uint      uvOffset;
    uint      colorOffset;
    uint      flags;        // bit0=hasNormal, bit1=hasUV, bit2=hasVertexColor, bit3=hasMaterialColor
    float     reflectivity;
};

static const uint FLAG_HAS_NORMAL        = 0x1;
static const uint FLAG_HAS_UV            = 0x2;
static const uint FLAG_HAS_VERTEX_COLOR  = 0x4;
static const uint FLAG_HAS_MATERIAL_COLOR = 0x8;

// Scene light. Must match vvh::Light (VHVulkan.h), std430 / 16-byte padded.
struct Light
{
    float3 positionW;  float _pad0;
    float3 directionW; float _pad1;
    float3 color;      float _pad2;
    float4 params;     // x=type(1=point,2=dir,3=spot), y=intensity, z=power, w=ambient
    float3 attenuation; float _pad3; // x=constant, y=linear, z=quadratic
};

// Primary ray payload carried through the ray tracing pipeline.
struct HitPayload
{
    float3 color;    // resulting radiance
    uint   depth;    // current recursion depth
};

// Shadow ray payload: set to false by the shadow miss shader (= not occluded).
struct ShadowPayload
{
    bool inShadow;
};

// --- Descriptor bindings (set 0) ---
[[vk::binding(0, 0)]] RaytracingAccelerationStructure topLevelAS;
[[vk::binding(1, 0)]] RWTexture2D<float4> outImage;
[[vk::binding(2, 0)]] ConstantBuffer<CameraRT> camera;
[[vk::binding(3, 0)]] StructuredBuffer<InstanceData> instances;
[[vk::binding(4, 0)]] StructuredBuffer<Light> lights;
[[vk::binding(5, 0)]] Sampler2D textures[];
