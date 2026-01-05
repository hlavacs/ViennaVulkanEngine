
import Defines;
import Materials;
import Lightsource;
import Random;
import Frame;
import BSDF;

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

struct IntersectionRecord
{
    float3 position;
    float3 direction;
    float3 normal;
    float2 uv;
    Frame frame;
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

struct IntegrationStepQueryRecord {
    float3 throughput = float3(1.0);
    float3 output = float3(0.0);
    RayDesc indirectRay = RayDesc();
}

IntersectionRecord getIntersectionInfo(in HitPayload payload, in RayDesc ray){
    
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

    its.uv = v0.uv.xy * payload.barycentrics.z + v1.uv.xy * payload.barycentrics.x + v2.uv.xy * payload.barycentrics.y;
    its.position = mul(instance.model, float4(localPosition, 1)).xyz;
    its.direction = ray.Direction;
    its.normal = normalize(mul(instance.modelInverse, float4(localNormal, 0)).xyz);
    its.materialIdx = instance.materialIndex;
    
    //incomming direction should be in the lower hemisphere.
    if (dot(its.normal, its.direction) > 0.0) {
        its.normal *= -1.0;
    }

    its.frame = Frame(its.normal);

    return its;
}

RayDesc getRay(in IntersectionRecord its, float3 castDirection) {
    RayDesc ray;

    ray.Origin = its.position;
    ray.Direction = normalize(castDirection);
    ray.TMin = 0.001;    // Minimum distance to avoid self-intersection
    ray.TMax = INFINITE; // Maximum distance (infinite for primary rays)

    return ray;
}

RayDesc getRay2(in IntersectionRecord its, float3 castDirection) {
    RayDesc ray;
    //normals are inverted
    ray.Origin = its.position + (its.normal * 0.001);
    ray.Direction = normalize(castDirection);
    ray.TMin = 0.0;    // Minimum distance to avoid self-intersection
    ray.TMax = INFINITE; // Maximum distance (infinite for primary rays)

    return ray;
}

RayDesc getRayInternal(in IntersectionRecord its, float3 castDirection) {
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

float3 directRaycast(in IntersectionRecord its, in MaterialRecord material, inout Sampler sampler) {
    EmitterRecord emitterRecord = sampleRandomLightSource(lightBuffer, its.position, sampler);

    RayDesc shadowRay = getRay(its, emitterRecord.direction);
    HitPayload payload;
    const uint rayFlags = 0;

    //self blocked
    
    if (dot(shadowRay.Direction, its.normal) < 0.0) {
        return float3(0.0, 0.0, 0.0);
    }
    

    TraceRay(topLevelAS, rayFlags, 0xff, 0, 0, 0, shadowRay, payload);

    if (payload.distance < emitterRecord.distance) {
        return float3(0.0, 0.0, 0.0);
    }

    BSDFQueryRecord shaderRecord;
    shaderRecord.wi = its.frame.toLocal(-its.direction);
    shaderRecord.wo = its.frame.toLocal(shadowRay.Direction);

    float3 bsdfEval = PrincipledEval(shaderRecord, material);

    return emitterRecord.sampleEmission * dot(its.normal, shadowRay.Direction) * bsdfEval;
}

RayDesc getInitialRay(in float2 launchID, in float2 launchSize) {
   

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

IntegrationStepQueryRecord simulateFirstBounce(inout Sampler sampler) {
    float2 launchID = (float2)DispatchRaysIndex().xy;        // Current pixel (x,y)
    float2 launchSize = (float2)DispatchRaysDimensions().xy; // Image size (width,height)

    IntersectionRecord its;

    RayDesc cameraRay = getInitialRay(launchID, launchSize);

    its.position = inPosition[int2(launchID)].xyz;
    its.normal = inNormal[int2(launchID)].xyz;
    its.frame = Frame(its.normal);
    its.direction = cameraRay.Direction;

    BSDFQueryRecord record;
    record.wi = -its.frame.toLocal(its.direction);

    MaterialRecord material;
    material.albedo = inAlbedoAlpha[int2(launchID)].xyz;
    material.alpha = inAlbedoAlpha[int2(launchID)].w;
    material.normal = inNormal[int2(launchID)].xyz;
    material.roughness = inSpec[int2(launchID)].x;
    material.metalness = inSpec[int2(launchID)].y;
    material.ior = inSpec[int2(launchID)].z;

    float3 directLight = directRaycast(its, material, sampler);

    float3 bsdf = samplePrincipled(record, material, sampler);

    RayDesc indirectRay = getRay(its, its.frame.toWorld(record.wo));

    IntegrationStepQueryRecord integrationStep;

    integrationStep.output = directLight;
    integrationStep.throughput = bsdf;
    integrationStep.indirectRay = indirectRay;

    return integrationStep;
}

bool isSkyPixel(float3 pos)
{
    return any(isinf(pos)) && any(pos < 0);
}

//Direct Integrator
float3 integratorDirect(inout Sampler sampler) {
    IntegrationStepQueryRecord integrationStep = simulateFirstBounce(sampler);

    return integrationStep.output;
}

float RRProbability(in float3 throughput) {
    float maxXY = max(throughput.x, throughput.y);
    float max = max(maxXY, throughput.z);

    return max;
}

float3 clipThroughput(in float3 throughput) {
    float3 throughputClipped;
    throughputClipped.x = min(throughput.x, 0.99);
    throughputClipped.y = min(throughput.y, 0.99);
    throughputClipped.z = min(throughput.z, 0.99);

    return throughputClipped;
}


float3 integratorPT(inout Sampler sampler) {
    IntegrationStepQueryRecord integrationStep = simulateFirstBounce(sampler);

    uint depth = 1;

    while (true) {
        float RRProb = RRProbability(integrationStep.throughput);

        HitPayload payload;
        const uint rayFlags = 0;
        TraceRay(topLevelAS, rayFlags, 0xff, 0, 0, 0, integrationStep.indirectRay, payload);

        if (!payload.hit) {
            break;
        }

        IntersectionRecord its = getIntersectionInfo(payload, integrationStep.indirectRay);

        BSDFQueryRecord shaderRecord;
        shaderRecord.wi = its.frame.toLocal(-its.direction);

        Material material = materials[its.materialIdx];
        MaterialRecord materialRecord = getMaterialProperties(material, textures, its.uv);

        float3 bsdf = samplePrincipled(shaderRecord, materialRecord, sampler);

        float3 bsdfEval = PrincipledEval(shaderRecord, materialRecord);



        integrationStep.indirectRay = getRayInternal(its, its.frame.toWorld(shaderRecord.wo));

        //check needed if material is descrete!!!
        /*
        if (bsdfQuery.measure != EDiscrete) {
            DirectRayCastQuerryRecord directLight = directLightRayCast(its, ray, scene, sampler);
            output += directLight.directSampleEmission * throughput;
        }
        */

        float3 directLight = directRaycast(its, materialRecord, sampler);
        integrationStep.output += directLight * integrationStep.throughput;

        if (sampler.UniformFloat() >= RRProb) {
            break;
        }

        integrationStep.throughput *= ((bsdf) / (RRProb));
        depth++;
    }

    return integrationStep.output;
}

[[vk::binding(0, 0)]]
cbuffer UniformBufferObject
{
    float4x4 view;
    float4x4 viewInv;
    float4x4 proj;
    float4x4 projInv;

    uint seed;
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



// Ray generation shader - entry point for each pixel
[shader("raygeneration")]
void rgenMain()
{
    
    // Get the current pixel coordinates and image dimensions
    float2 launchID = (float2)DispatchRaysIndex().xy;     // Current pixel (x,y)
    float2 launchSize = (float2)DispatchRaysDimensions().xy; // Image size (width,height)

    /*
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

    if (payload.hit) {
        IntersectionRecord its = getIntersectionInfo(payload, ray);

        if (dot(ray.Direction, its.normal) > 0.0) {
            outputColor = float3(0.0, 0.0, 1.0);
        } else {
            outputColor = float3(1.0, 0.0, 0.0);
        }
        
    }
     outImage[int2(launchID)] = float4(outputColor, 1.0);
     */

    
    float3 position = inPosition[int2(launchID)].xyz;

    if (isSkyPixel(position)) {
        outImage[int2(launchID)] = float4(0.0, 0.0, 0.0, 1.0);
    } else {
        uint linear = (uint)launchID.x + (uint)launchID.y * (uint)launchSize.x;
        uint pixelSeed = Hash(linear ^ (seed * 0x9E3779B9u));

        Sampler sampler = Sampler(pixelSeed);

        float3 outputColor = integratorPT(sampler);
        outImage[int2(launchID)] = float4(outputColor, 1.0);
    }
    
    

    //outImage[int2(launchID)] = float4(outputColor, 1.0);
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
    payload.distance = INFINITE;
    payload.color = float3(0.5, 0.5, 0.5); // Gray background
}