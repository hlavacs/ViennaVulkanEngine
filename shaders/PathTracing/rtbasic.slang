
import Materials;

struct Vertex
{
    float4 position;
    float4 normal;
    float4 uv;
};

struct Instance
{
float4x4 model;
float4x4 modelInverse;
uint materialIndex;
uint firstIndex;
float2 uvScale;
};

struct LightSource
{
    float4 position;
    float4 emission;
    float4 direction;
    float radius;
    float pdf;
    float accumulativeSampleWeight;
    uint lightType;
};

struct IntersectionInfo
{
 float3 position;
 float3 normal;
 float2 uv;
 uint materialIdx;
};

// Ray payload structure - carries data through the ray tracing pipeline
struct HitPayload
{
    bool hit;
    float3 color;
    float distance;
    uint triangleIdx;
    uint instanceIdx;
    float3 barycentrics;
};


IntersectionInfo getIntersectionInfo(in HitPayload payload){
    
    Instance instance = instanceBuffer[payload.instanceIdx];
    uint firstIndex = instance.firstIndex;

    uint i0 = indexBuffer[firstIndex + payload.triangleIdx * 3 + 0];
    uint i1 = indexBuffer[firstIndex + payload.triangleIdx * 3 + 1];
    uint i2 = indexBuffer[firstIndex + payload.triangleIdx * 3 + 2];

    Vertex v0 = vertexBuffer[i0];
    Vertex v1 = vertexBuffer[i1];
    Vertex v2 = vertexBuffer[i2];

    

    IntersectionInfo its;

    float3 localPosition = v0.position.xyz * payload.barycentrics.z + v1.position.xyz * payload.barycentrics.x + v2.position.xyz * payload.barycentrics.y;
    float3 localNormal = normalize(v0.normal.xyz * payload.barycentrics.z + v1.normal.xyz * payload.barycentrics.x + v2.normal.xyz * payload.barycentrics.y);

    its.uv = v0.uv.xy * payload.barycentrics.z + v1.uv.xy * payload.barycentrics.x + v2.uv.xy * payload.barycentrics.y;
    its.position = mul(instance.model, float4(localPosition, 1)).xyz;
    its.normal = normalize(mul(instance.modelInverse, float4(localNormal, 0)).xyz);
    its.materialIdx = instance.materialIndex;

    return its;
}

[[vk::binding(0, 0)]]
cbuffer UniformBufferObject
{
    float4x4 view;
    float4x4 viewInv;
    float4x4 proj;
    float4x4 projInv;
};

[[vk::binding(1, 0)]]
StructuredBuffer<Material> materials;


[[vk::binding(0, 1)]]
RaytracingAccelerationStructure topLevelAS;

[[vk::binding(1, 1)]]
StructuredBuffer<Vertex> vertexBuffer;

[[vk::binding(2, 1)]]
StructuredBuffer<uint> indexBuffer;

[[vk::binding(3, 1)]]
StructuredBuffer<Instance> instanceBuffer;

[[vk::binding(4, 1)]]
StructuredBuffer<LightSource> lightBuffer;




[[vk::binding(0, 2)]]
RWTexture2D<float4> inAlbedoAlpha;

[[vk::binding(1, 2)]]
RWTexture2D<float4> inNormal;

[[vk::binding(2, 2)]]
RWTexture2D<float4> inSpec;

[[vk::binding(3, 2)]]
RWTexture2D<float4> inPosition;

[[vk::binding(4, 2)]]
RWTexture2D<float4> outImage;

static const float INFINITE = 1e30;




// Ray generation shader - entry point for each pixel
[shader("raygeneration")]
void rgenMain()
{
    // Get the current pixel coordinates and image dimensions
    float2 launchID = (float2)DispatchRaysIndex().xy;     // Current pixel (x,y)
    float2 launchSize = (float2)DispatchRaysDimensions().xy; // Image size (width,height)

    // Set ray tracing flags (0 = no special flags)
    const uint rayFlags = 0;
    
    // Convert pixel coordinates to normalized device coordinates (NDC)
    // Range: [-1,1] for both x and y
    const float2 clipCoords = launchID / launchSize * 2.0 - 1.0;
    
    // Transform from NDC to view space using inverse projection matrix
    const float4 viewCoords = mul(float4(clipCoords, 1.0, 1.0), projInv);

    const float4 viewCoordsHomogeneous = mul(float4(clipCoords, 1.0, 1.0), projInv);

    // CRITICAL FIX: Divide X, Y, Z by W to get correct view space coordinates
    const float3 viewRayDir = viewCoordsHomogeneous.xyz / viewCoordsHomogeneous.w;

    // Create the primary ray
    RayDesc ray;
    // Origin calculation (This is correct)
    ray.Origin = mul(float4(0.0, 0.0, 0.0, 1.0), viewInv).xyz; 

    // DIRECTION FIX: Use the corrected viewRayDir
    ray.Direction = mul(float4(normalize(viewRayDir), 0.0), viewInv).xyz;

    ray.TMin = 0.001;    // Minimum distance to avoid self-intersection
    ray.TMax = INFINITE; // Maximum distance (infinite for primary rays)

    // Initialize ray payload with default values
    HitPayload payload;
    payload.color = float3(0, 0, 0);  // Start with black

    // Cast the ray into the scene using the acceleration structure
    // Parameters: AS, flags, instance mask, sbt offset, sbt stride, miss offset, ray, payload
    TraceRay(topLevelAS, rayFlags, 0xff, 0, 0, 0, ray, payload);
    
    // Get the final color from the ray tracing result
    float3 color = payload.color;
    
    // Write the result to the output image
    //outImage[int2(launchID)] = float4(color, 1.0);

    float3 outputColor = float3(0.0,0.0,0.0);

    if(payload.hit){
        IntersectionInfo its =  getIntersectionInfo(payload);

        outputColor = float3(its.uv,0);

    }
    
    outImage[int2(launchID)] = float4(outputColor, 1.0);
    //outImage[int2(launchID)] = float4(inPosition[int2(launchID)].xyz, 1.0);
    //outImage[int2(launchID)] = float4(inNormal[int2(launchID)].xyz, 1.0);
}

// Closest hit shader - called when ray hits geometry
[shader("closesthit")]
void rchitMain(inout HitPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    payload.hit = true;
    payload.distance = RayTCurrent();
    payload.instanceIdx = InstanceIndex();
    payload.triangleIdx = PrimitiveIndex();
    payload.barycentrics = float3(attr.barycentrics.x, attr.barycentrics.y, 1.0 - attr.barycentrics.x - attr.barycentrics.y);

    //uint firstIndex = InstanceID(); This is how to access the costom index

    payload.color = float3(1, 0, 0); // Red color
}

// Miss shader - called when ray doesn't hit any geometry
[shader("miss")]
void rmissMain(inout HitPayload payload)
{
    // Set background color when ray misses all geometry
    // In a full implementation, this could sample from:
    // - Skybox textures
    // - Procedural sky gradients
    // - Environment maps
    payload.hit = false;
    payload.color = float3(0.5, 0.5, 0.5); // Gray background
}