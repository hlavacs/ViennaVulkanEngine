// Whitted closest hit shader for VERendererRaytracing.
// Computes local Phong shading with hard shadows (shadow rays) and adds a
// recursive reflection contribution for reflective materials.
#include "common.slang"

// --- Per-vertex attribute fetch through buffer-device-address pointers ---

uint3 getIndices(InstanceData inst, uint prim)
{
    return uint3(inst.indexBuffer[prim * 3 + 0],
                 inst.indexBuffer[prim * 3 + 1],
                 inst.indexBuffer[prim * 3 + 2]);
}

// Note: *Offset fields are byte offsets into the interleaved buffer; the buffer
// is addressed as float32 elements, so divide byte offsets/strides by 4.
float3 getPosition(InstanceData inst, uint idx)
{
    const uint b = idx * 3;
    return float3(inst.vertexBuffer[b + 0], inst.vertexBuffer[b + 1], inst.vertexBuffer[b + 2]);
}

float3 getNormal(InstanceData inst, uint idx)
{
    const uint b = inst.normalOffset / 4 + idx * 3;
    return float3(inst.vertexBuffer[b + 0], inst.vertexBuffer[b + 1], inst.vertexBuffer[b + 2]);
}

float2 getTexCoord(InstanceData inst, uint idx)
{
    const uint b = inst.uvOffset / 4 + idx * 2;
    return float2(inst.vertexBuffer[b + 0], inst.vertexBuffer[b + 1]);
}

float4 getColor(InstanceData inst, uint idx)
{
    const uint b = inst.colorOffset / 4 + idx * 4;
    return float4(inst.vertexBuffer[b + 0], inst.vertexBuffer[b + 1],
                  inst.vertexBuffer[b + 2], inst.vertexBuffer[b + 3]);
}

// Traces a shadow ray towards a light, returns true if the point is occluded.
bool traceShadow(float3 origin, float3 dir, float tMax)
{
    RayDesc ray;
    ray.Origin = origin;
    ray.Direction = dir;
    ray.TMin = 0.001;
    ray.TMax = tMax;

    ShadowPayload sp;
    sp.inShadow = true; // assume occluded; the shadow miss shader clears it.

    // Trace shadow rays with the ShadowRay mask bit (0x02) only. Light-marker
    // instances have this bit cleared in the TLAS, so they are skipped here and
    // never self-occlude their own light, while real geometry still casts shadows.
    TraceRay(topLevelAS,
             RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER,
             0x02,
             0, // hit group offset (skipped anyway)
             0,
             1, // shadow miss shader index
             ray,
             sp);
    return sp.inShadow;
}

[shader("closesthit")]
void rchitMain(inout HitPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    const InstanceData inst = instances[InstanceID()];

    const uint3 idx = getIndices(inst, PrimitiveIndex());
    const float3 bc = float3(1.0 - attr.barycentrics.x - attr.barycentrics.y,
                       attr.barycentrics.x,
                       attr.barycentrics.y);

    // World space hit position.
    const float3 P = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();

    // Interpolated, world space normal.
    float3 nObj;
    if ((inst.flags & FLAG_HAS_NORMAL) != 0)
    {
        nObj = getNormal(inst, idx.x) * bc.x +
               getNormal(inst, idx.y) * bc.y +
               getNormal(inst, idx.z) * bc.z;
    }
    else
    {
        // Geometric normal from the triangle positions.
        const float3 p0 = getPosition(inst, idx.x);
        const float3 p1 = getPosition(inst, idx.y);
        const float3 p2 = getPosition(inst, idx.z);
        nObj = cross(p1 - p0, p2 - p0);
    }
    // Inverse-transpose transform: mul(normal, WorldToObject) == (W2O)^T * n.
    float3 N = normalize(mul(nObj, (float3x3)WorldToObject4x3()));

    const float3 V = normalize(camera.cameraPos.xyz - P);
    // Make the normal face the viewer (handles back faces / inside of objects).
    if (dot(N, V) < 0.0) { N = -N; }

    // --- Base albedo: material color, vertex color, then texture modulation ---
    float3 albedo = float3(1.0, 1.0, 1.0);
    if ((inst.flags & FLAG_HAS_MATERIAL_COLOR) != 0)
    {
        albedo = inst.diffuse.rgb;
    }
    else if ((inst.flags & FLAG_HAS_VERTEX_COLOR) != 0)
    {
        albedo = (getColor(inst, idx.x) * bc.x +
                  getColor(inst, idx.y) * bc.y +
                  getColor(inst, idx.z) * bc.z).rgb;
    }

    if (inst.textureIndex >= 0 && (inst.flags & FLAG_HAS_UV) != 0)
    {
        const float2 uv = (getTexCoord(inst, idx.x) * bc.x +
                     getTexCoord(inst, idx.y) * bc.y +
                     getTexCoord(inst, idx.z) * bc.z) * inst.uvScale;
        albedo *= textures[inst.textureIndex].SampleLevel(uv, 0).rgb;
    }

    const float3 specularColor = inst.specular.rgb;
    const float shininess = max(inst.specular.w, 1.0);

    // --- Local Phong shading with shadow rays ---
    float3 result = albedo * 0.03; // small constant ambient

    const int total = camera.numLights.w;
    for (int i = 0; i < total; ++i)
    {
        const Light light = lights[i];
        const float type = light.params.x;
        const float intensity = light.params.y;

        float3 L;
        float tMax;
        float atten = 1.0;
        if (type == 2.0)
        {
            // Directional light.
            L = normalize(-light.directionW);
            tMax = 10000.0;
        }
        else
        {
            // Point / spot light.
            const float3 toLight = light.positionW - P;
            const float dist = length(toLight);
            L = toLight / max(dist, 1e-4);
            tMax = dist - 0.001;
            atten = 1.0 / (light.attenuation.x +
                           light.attenuation.y * dist +
                           light.attenuation.z * dist * dist);
        }

        // Ambient term contributed per light.
        result += albedo * light.color * light.params.w * 0.1;

        const float ndotl = max(dot(N, L), 0.0);
        if (ndotl <= 0.0) { continue; }

        // Shadow test.
        if (traceShadow(P + N * 0.001, L, tMax)) { continue; }

        const float3 radiance = light.color * intensity * atten;

        // Diffuse.
        result += albedo * ndotl * radiance;

        // Specular (Phong).
        const float3 R = reflect(-L, N);
        const float spec = pow(max(dot(R, V), 0.0), shininess);
        result += specularColor * spec * radiance;
    }

    // --- Recursive reflection ---
    if (inst.reflectivity > 0.0 && payload.depth + 1 < MAX_RAY_DEPTH)
    {
        RayDesc rray;
        rray.Origin = P + N * 0.001;
        rray.Direction = reflect(WorldRayDirection(), N);
        rray.TMin = 0.001;
        rray.TMax = 10000.0;

        HitPayload rpayload;
        rpayload.color = float3(0.0, 0.0, 0.0);
        rpayload.depth = payload.depth + 1;

        TraceRay(topLevelAS, RAY_FLAG_NONE, 0xFF, 0, 0, 0, rray, rpayload);

        result = lerp(result, rpayload.color, inst.reflectivity);
    }

    payload.color = result;
}
