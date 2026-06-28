import Defines;
import Materials;
import Lightsource;
import Random;
import Frame;
import BSDF;
import ColorSpace;
import Surface;

struct Vertex
{
    float4 position;
    float4 normal;
    float4 uv;
    float4 tangent;
};

struct Instance
{
    float4x4 model;
    float4x4 modelInverse;
    uint materialIndex;
    uint firstIndex;
    float2 uvScale;
};

[[vk::binding(0, 0)]]
cbuffer UniformBufferObject
{
    float4x4 view;
    float4x4 viewInv;
    float4x4 proj;
    float4x4 projInv;

    float4x4 prevView;
    float4x4 prevProj;

    uint seed;
    uint lightCount;
    uint x_dimensions;
    uint y_dimensions;

    uint isFirstFrame;
    uint illuminationDomain;
    float accumulationFactor;
    uint directSampleCount;

    uint indirectSampleCountForward;
    uint indirectSampleCountBidirectional;
    uint candidateSamplesRestirDi;
    uint spatialSamplesRestirDI;

    uint spatialRadiusRestirDI;
    uint candidateSamplesRestirGI;
    uint spatialSamplesRestirGI;
    uint spatialRadiusRestirGI;

    uint candidateSamplesRestirLVC;
    uint spatialSamplesRestirLVC;
    uint spatialRadiusRestirLVC;
};

[[vk::binding(1, 0)]]
StructuredBuffer<Material> materials;

[[vk::binding(2, 0)]]
Sampler2D textures[];


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

// Ray payload structure - carries data through the ray tracing pipeline
struct HitPayload
{
    // Outputs: hit data returned from ray traversal.
    bool hit;
    float3 color;
    float distance;
    uint triangleIdx;
    uint instanceIdx;
    float3 barycentrics;
};

struct IntegrationStepQueryRecord {
    // Outputs: per-bounce integration state.
    float3 throughput;
    float3 directLight;
    float3 indirectLight;
    RayDesc indirectRay;
}

IntersectionRecord getIntersectionInfo(in HitPayload payload, in RayDesc ray) {
    // Inputs: hit payload and ray. Output: resolved intersection record in world space.

    Instance instance = instanceBuffer[payload.instanceIdx];
    uint firstIndex = instance.firstIndex;

    uint i0 = indexBuffer[firstIndex + payload.triangleIdx * 3 + 0];
    uint i1 = indexBuffer[firstIndex + payload.triangleIdx * 3 + 1];
    uint i2 = indexBuffer[firstIndex + payload.triangleIdx * 3 + 2];

    Vertex v0 = vertexBuffer[i0];
    Vertex v1 = vertexBuffer[i1];
    Vertex v2 = vertexBuffer[i2];

    IntersectionRecord its;

    float3 localPosition = v0.position.xyz * payload.barycentrics.z + v1.position.xyz * payload.barycentrics.x + v2.position.xyz * payload.barycentrics.y;
    float3 localNormal = normalize(v0.normal.xyz * payload.barycentrics.z + v1.normal.xyz * payload.barycentrics.x + v2.normal.xyz * payload.barycentrics.y);

    float3 localTangent = normalize(v0.tangent.xyz * payload.barycentrics.z + v1.tangent.xyz * payload.barycentrics.x + v2.tangent.xyz * payload.barycentrics.y);

    its.uv = v0.uv.xy * payload.barycentrics.z + v1.uv.xy * payload.barycentrics.x + v2.uv.xy * payload.barycentrics.y;
    its.uv *= instance.uvScale;
    its.position = mul(instance.model, float4(localPosition, 1)).xyz;
    its.direction = ray.Direction;
    its.normal = normalize(mul(instance.modelInverse, float4(localNormal, 0)).xyz);
    its.tangent = normalize(mul(instance.modelInverse, float4(localTangent, 0)).xyz);
    its.materialIdx = instance.materialIndex;

    // incomming direction should be in the lower hemisphere.
    if (dot(its.normal, its.direction) > 0.0) {
        its.normal *= -1.0;
    }

    its.frame = Frame(its.normal, its.tangent);

    return its;
}

RayDesc getRay(in IntersectionRecord its, float3 castDirection) {
    // Inputs: intersection and cast direction. Output: offset ray starting at surface.
    RayDesc ray;

    ray.Origin = its.position;
    ray.Direction = normalize(castDirection);
    ray.TMin = 0.001;    // Minimum distance to avoid self-intersection
    ray.TMax = INFINITE; // Maximum distance (infinite for primary rays)

    return ray;
}

RayDesc getRay2(in IntersectionRecord its, float3 castDirection) {
    // Inputs: intersection and cast direction. Output: ray offset along normal.
    RayDesc ray;
    // normals are inverted
    ray.Origin = its.position + (its.normal * 0.001);
    ray.Direction = normalize(castDirection);
    ray.TMin = 0.0;      // Minimum distance to avoid self-intersection
    ray.TMax = INFINITE; // Maximum distance (infinite for primary rays)

    return ray;
}

RayDesc getRayInternal(in IntersectionRecord its, float3 castDirection) {
    // Inputs: intersection and cast direction. Output: ray offset based on side.
    RayDesc ray;
    if (dot(its.normal, castDirection) > 0.0) {
        ray.Origin = its.position + (its.normal * 0.001);
    } else {
        ray.Origin = its.position - (its.normal * 0.001);
    }

    ray.Direction = normalize(castDirection);
    ray.TMin = 0.0;      // Minimum distance to avoid self-intersection
    ray.TMax = INFINITE; // Maximum distance (infinite for primary rays)

    return ray;
}

float3 offset_ray(float3 p, float3 n) {
    const float epsilon = 1.0 / 32.0;
    const float float_scale = 1.0 / 65536.0;
    const float int_scale = 256.0;

    int3 i = int3(n * int_scale);
    float3 p_i = asfloat(asint(p) + select(p < 0, -i, i));
    return select(abs(p) < epsilon, p + float_scale * n, p_i);
}

RayDesc getRayOffset(in IntersectionRecord its, float3 castDirection) {
    // Inputs: intersection and cast direction. Output: ray offset based on side.
    RayDesc ray;
    // ray.Origin = its.position + (its.normal * 0.001);
    ray.Origin = offset_ray(its.position, its.normal);

    ray.Direction = normalize(castDirection);
    ray.TMin = 0.0;      // Minimum distance to avoid self-intersection
    ray.TMax = INFINITE; // Maximum distance (infinite for primary rays)

    return ray;
}

RayDesc getInitialRay(in float2 launchID, in float2 launchSize) {
    // Inputs: pixel coordinates and image size. Output: camera ray in world space.

    const float2 clipCoords = launchID / launchSize * 2.0 - 1.0;
    const float4 viewCoordsHomogeneous = mul(float4(clipCoords, 1.0, 1.0), projInv);
    const float3 viewRayDir = viewCoordsHomogeneous.xyz / viewCoordsHomogeneous.w;

    RayDesc ray;
    ray.Origin = mul(float4(0.0, 0.0, 0.0, 1.0), viewInv).xyz;
    ray.Direction = mul(float4(normalize(viewRayDir), 0.0), viewInv).xyz;
    ray.TMin = 0.001;
    ray.TMax = INFINITE;

    return ray;
}

bool isSkyPixel(float3 pos)
{
    // Input: position sample. Output: true if position encodes sky.
    return any(isinf(pos)) && any(pos < 0);
}

bool isLightOccluded(in IntersectionRecord its, EmitterRecord emitterRecord) {
    RayDesc shadowRay = getRayOffset(its, emitterRecord.direction);
    shadowRay.TMax = emitterRecord.distance;
    HitPayload payload;
    const uint rayFlags = 0;

    // self blocked
    if (dot(shadowRay.Direction, its.normal) < 0.0) {
        return true;
    }

    TraceRay(topLevelAS, rayFlags, 0xff, 0, 0, 0, shadowRay, payload);

    if (payload.distance < emitterRecord.distance) {
        return true;
    }

    return false;
}

// this code sometimes still returns false posiitives iit is completly unclear why
bool isSurfacePointOccluded(in IntersectionRecord its, EmitterRecord emitterRecord) {
    // 1. Use a robust offset to prevent self-intersection
    RayDesc shadowRay;
    // shadowRay.Origin = its.position + (its.normal * 0.001);
    shadowRay.Origin = offset_ray(its.position, its.normal);
    shadowRay.Direction = normalize(emitterRecord.direction);
    shadowRay.TMin = 0.0;
    // 2. Stop the ray just BEFORE it reaches the target point
    shadowRay.TMax = emitterRecord.distance - 0.001;

    // 3. Horizon check (using a small epsilon to allow grazing lights)
    if (dot(shadowRay.Direction, its.normal) < 1e-4f) {
        return true;
    }

    HitPayload payload;
    payload.hit = false; // Ensure this is initialized!

    // 4. Optimization: Use flags to stop at the very first hit
    const uint rayFlags = RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH;

    TraceRay(topLevelAS, rayFlags, 0xff, 0, 0, 0, shadowRay, payload);

    // If the ray hit anything before reaching the target, it's occluded
    return payload.hit;
}

float3 getUnoccludedLightContribution(in IntersectionRecord its, in MaterialRecord material, in EmitterRecord emitterRecord) {
    BSDFQueryRecord shaderRecord = constructBSDFRecord(its, material, emitterRecord.direction, true);

    float3 bsdfEval = PrincipledEval(shaderRecord, material);

    float3 radiance = emitterRecord.sampleEmission * bsdfEval;

    if (any(isnan(radiance)) || any(isinf(radiance))) {
        radiance = float3(0.0);
    }

    // bsdf can return negativ values !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    //fix this
    return max(0.0, radiance);
}

float3 directRaycast(in IntersectionRecord its, in MaterialRecord material, inout Sampler sampler) {
    // Inputs: intersection, material, RNG. Output: direct lighting contribution.
    EmitterRecord emitterRecord = sampleRandomLightSourceWeighted(lightBuffer, its.position, sampler);

    bool occluded = isLightOccluded(its, emitterRecord);

    if (occluded) {
        return float3(0.0, 0.0, 0.0);
    }

    return getUnoccludedLightContribution(its, material, emitterRecord);
}
