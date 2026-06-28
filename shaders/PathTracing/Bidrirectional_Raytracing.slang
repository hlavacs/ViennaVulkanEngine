import Warp;
import raytracing;
import Lightsource;
import Random;
import Frame;
import Defines;
import Surface;
import Materials;
import BSDF;
import LightVertex;

[[vk::binding(5, 1)]]
RWStructuredBuffer<LightVertex> lightVertexCache;

[[vk::binding(6, 1)]]
cbuffer UniformBufferBidirectional
{
    uint lightVertexCacheSize;
};

/*
float lightVertexPDF(in StructuredBuffer<LightSource> lightSources, in LightVertex lightVertex) {
    //float distance = length(lightVertex.samplePosition - lightVertex.hitPosition);
    return lightSourcePdf(lightBuffer, lightVertex.lightIndex);
}
    */

float lightVertexPDF(in StructuredBuffer<LightSource> lightSources, in LightVertex lightVertex) {
    // float distance = length(lightVertex.samplePosition - lightVertex.hitPosition);
    float pdf = lightSourcePdf(lightBuffer, lightVertex.lightIndex);

    float distance = length(lightVertex.samplePosition - lightVertex.hitPosition);

    LightSource light = lightBuffer[lightVertex.lightIndex];

    float lightArea = 4.0 * light.radius * light.radius * M_PI;

    float lightDirectionSelectionPdf = dot(normalize(lightVertex.samplePosition - light.position), normalize(lightVertex.hitPosition - lightVertex.samplePosition)) / (M_PI);
    float cosinLtoLightSourceOnL = max(0.0, dot(lightVertex.normal, normalize(lightVertex.samplePosition - lightVertex.hitPosition)));

    return pdf * (1.0 / lightArea) * lightDirectionSelectionPdf * (cosinLtoLightSourceOnL / (distance * distance));
}

LightVertex generateLightVertex(inout Sampler sampler) {
    LightVertex lightVertex;

    float pdf = 0.0;
    uint lightIndex = 0;
    LightSource light = chooseRandomLightSourceWeighted(lightBuffer, sampler, pdf, lightIndex);

    float2 sample = sampler.UniformFloat2();
    float3 lightSurfaceDirection = squareToUniformSphere(sample);
    float lightSurfaceSelectionPdf = squareToUniformSpherePdf(lightSurfaceDirection);
    float3 lightSurfacePosition = light.position.xyz + (lightSurfaceDirection * light.radius);

    float lightArea = 4.0 * light.radius * light.radius * M_PI;
    Frame lightFrame = Frame(lightSurfaceDirection);

    sample = sampler.UniformFloat2();
    float3 lightDirectionLocal = squareToCosineHemisphere(sample);
    float lightDirectionSelectionPdf = squareToCosineHemispherePdf(lightDirectionLocal);
    float3 lightSampleDirection = lightFrame.toWorld(lightDirectionLocal);
    float cosLightSource = dot(lightFrame.n, lightSampleDirection);

    RayDesc ray;
    ray.Origin = lightSurfacePosition;
    ray.Direction = normalize(lightSampleDirection);
    ray.TMin = 0.0;
    ray.TMax = INFINITE;

    HitPayload payload;
    const uint rayFlags = 0;
    TraceRay(topLevelAS, rayFlags, 0xff, 0, 0, 0, ray, payload);

    if (!payload.hit) {
        lightVertex.radiance = float4(0.0);
        lightVertex.age = 0;
        lightVertex.hitPosition = float4(0.0);
        lightVertex.importance = 0.0;
        lightVertex.lightIndex = lightIndex;
        lightVertex.samplePosition = float4(0.0);
        lightVertex.selectionPdf = 0.0;

        lightVertex.normal = float4(0.0);
        lightVertex.tangent = float4(0.0);
        lightVertex.materialIndex = 0;
        lightVertex.uv = float2(0.0);

        return lightVertex;
    }

    IntersectionRecord its = getIntersectionInfo(payload, ray);

    lightVertex.age = 0;
    lightVertex.hitPosition = float4(its.position, 1.0);
    lightVertex.importance = 0.0;
    lightVertex.lightIndex = lightIndex;

    float distance = length(its.position - lightSurfacePosition);
    float cosinLtoLightSourceOnL = max(0.0,dot(its.normal, -normalize(lightSampleDirection)));

    // we ask how much light does this one point emmit into the given direction!
    //move the cosine to the geometry term for consistency
    //light area cancels out with the pdf the light source does not get brighter because it gets larger in this render engine!
    //the cosine should also cancel out with the cosine in the pdf
    lightVertex.radiance = light.emission / lightArea;

    lightVertex.samplePosition = float4(lightSurfacePosition, 1.0);

    // We still keep the tracking PDF exactly the same for MIS calculations later!
    // probability of choosing point on light source * the probability of choosing the light vertex
    // lightDirectionSelectionPdf will cancel out cosThetaOnLight
    // cosinLtoLightSourceOnL will later cancel out during rendering
    // cosinLtoLightSourceOnL
    lightVertex.selectionPdf = pdf * (1.0 / lightArea) * lightDirectionSelectionPdf * (cosinLtoLightSourceOnL / (distance * distance));

    /*
    // 1. Area PDF of choosing the specific point on the light source
    float p_y0 = pdf / lightArea;

    // 2. Solid angle PDF of the cosine-weighted direction
    float p_omega = cosLightSource / M_PI;

    // 3. Conversion factor from solid angle to area measure at the hit surface
    // Note: We use max(0.0001, distance * distance) to prevent division by zero in tight corners
    float distSq = max(0.0001, distance * distance);
    float geometryFactor = cosinLtoLightSourceOnL / distSq;

    // 4. The full area PDF of generating this specific light vertex from the specific point on the light
    lightVertex.selectionPdf = p_y0 * p_omega * geometryFactor;
    */


    lightVertex.normal = float4(its.normal, 1.0);
    lightVertex.tangent = float4(its.tangent, 1.0);
    lightVertex.materialIndex = its.materialIdx;
    lightVertex.uv = its.uv;

    return lightVertex;
}

/*
LightVertex generateLightVertex(inout Sampler sampler) {
    LightVertex lightVertex;

    float pdf = 0.0;
    uint lightIndex = 0;
    LightSource light = chooseRandomLightSourceWeighted(lightBuffer, sampler, pdf, lightIndex);

    float2 sample = sampler.UniformFloat2();
    float3 lightSurfaceDirection = squareToUniformSphere(sample);
    float lightSurfaceSelectionPdf = squareToUniformSpherePdf(lightSurfaceDirection);
    float3 lightSurfacePosition = light.position.xyz + (lightSurfaceDirection * light.radius);

    float lightArea = 4.0 * light.radius * light.radius * M_PI;
    Frame lightFrame = Frame(lightSurfaceDirection);

    sample = sampler.UniformFloat2();
    float3 lightDirectionLocal = squareToCosineHemisphere(sample);
    float lightDirectionSelectionPdf = squareToCosineHemispherePdf(lightDirectionLocal);
    float3 lightSampleDirection = lightFrame.toWorld(lightDirectionLocal);

    RayDesc ray;
    ray.Origin = lightSurfacePosition;
    ray.Direction = normalize(lightSampleDirection);
    ray.TMin = 0.0;
    ray.TMax = INFINITE;

    HitPayload payload;
    const uint rayFlags = 0;
    TraceRay(topLevelAS, rayFlags, 0xff, 0, 0, 0, ray, payload);

    if (!payload.hit) {
        // ... (Keep your old miss shader initialization, but set radiance to 0.0)
        lightVertex.radiance = float4(0.0);
        return lightVertex;
    }

    IntersectionRecord its = getIntersectionInfo(payload, ray);

    lightVertex.age = 0;
    lightVertex.hitPosition = float4(its.position, 1.0);
    lightVertex.importance = 0.0;
    lightVertex.lightIndex = lightIndex;

    float distance = length(its.position - lightSurfacePosition);
    float cosHit = saturate(dot(its.normal, -normalize(lightSampleDirection)));

    // FIX 1: Store the true path throughput (alpha^L) instead of raw emission.
    // This is the total power of the path prefix divided by the emission PDFs.
    lightVertex.radiance = float4((light.emission.xyz * lightArea * M_PI) / pdf, 1.0);

    lightVertex.samplePosition = float4(lightSurfacePosition, 1.0);

    // We still keep the tracking PDF exactly the same for MIS calculations later!
    lightVertex.selectionPdf = pdf * (1.0 / lightArea) * lightDirectionSelectionPdf * (cosHit / (distance * distance));


    lightVertex.normal = float4(its.normal, 1.0);
    lightVertex.tangent = float4(its.tangent, 1.0);
    lightVertex.materialIndex = its.materialIdx;
    lightVertex.uv = its.uv;

    return lightVertex;
}
*/

bool isLightVertexValid(uint lightVertexIndex) {
    LightVertex lightVertex = lightVertexCache[lightVertexIndex];
    return length(lightVertex.radiance) >= 0.0;
}

IntersectionRecord getIntersectionRecordFromLightVertex(in LightVertex lightVertex, in IntersectionRecord its) {
    IntersectionRecord itsConnection;

    // its.direction is the reay direction aka ingoing instead of outgoing bsdf inverts this
    // itsConnection.direction = normalize(its.position - lightVertex.hitPosition.xyz);
    itsConnection.direction = normalize(lightVertex.hitPosition.xyz - its.position);
    itsConnection.materialIdx = lightVertex.materialIndex;
    itsConnection.normal = lightVertex.normal.xyz;
    itsConnection.position = lightVertex.hitPosition.xyz;
    itsConnection.tangent = lightVertex.tangent.xyz;
    itsConnection.frame = Frame(itsConnection.normal, itsConnection.tangent);
    return itsConnection;
}

EmitterRecord getEmitterRecordFromLightVertex(in LightVertex lightVertex) {
    EmitterRecord emitterRecord;
    emitterRecord.direction = normalize(lightVertex.samplePosition.xyz - lightVertex.hitPosition.xyz);
    emitterRecord.distance = length(lightVertex.samplePosition.xyz - lightVertex.hitPosition.xyz);
    emitterRecord.intersection = lightVertex.hitPosition.xyz;

    float distSq = max(emitterRecord.distance * emitterRecord.distance, 0.01);

    LightSource light = lightBuffer[lightVertex.lightIndex];
    float cosThetaOnLight = dot(normalize(lightVertex.samplePosition - light.position), normalize(lightVertex.hitPosition - lightVertex.samplePosition));

    emitterRecord.sampleEmission = (lightVertex.radiance.xyz * cosThetaOnLight / distSq) / lightVertex.selectionPdf;

    return emitterRecord;
}


EmitterRecord getEmitterRecordFromConnection(in EmitterRecord fromLightSource, in IntersectionRecord its, in IntersectionRecord itsConnection, in MaterialRecord materialRecordConnection) {
    EmitterRecord emitterRecord;
    emitterRecord.direction = normalize(itsConnection.position - its.position);
    emitterRecord.distance = length(itsConnection.position - its.position);
    emitterRecord.intersection = itsConnection.position;
    float3 bsdfEvalConnection = getUnoccludedLightContribution(itsConnection, materialRecordConnection, fromLightSource);

    float cosThetaLight = saturate(dot(itsConnection.normal, -emitterRecord.direction));

    float distSq = max(emitterRecord.distance * emitterRecord.distance, 0.01);

    emitterRecord.sampleEmission = bsdfEvalConnection * cosThetaLight / distSq;

    return emitterRecord;
}

float3 getUnoccludedLightContributionFromConnection(in IntersectionRecord its, in MaterialRecord material, in uint lightVertexIndex) {

    LightVertex lightVertex = lightVertexCache[lightVertexIndex];

    if (length(lightVertex.radiance) <= 0.0) {
        return float3(0.0);
    }

    IntersectionRecord itsConnection = getIntersectionRecordFromLightVertex(lightVertex, its);

    //low distance is clearly a problem but better ffix needed!
    if (length(itsConnection.position - its.position) < 0.01) {
        return float3(0.0);
    }


    Material materialConnection = materials[itsConnection.materialIdx];
    MaterialRecord materialRecordConnection = getMaterialProperties(materialConnection, textures, itsConnection.uv, itsConnection.frame);

    EmitterRecord emitterRecord = getEmitterRecordFromLightVertex(lightVertex);
    EmitterRecord emitterRecordFromConnection = getEmitterRecordFromConnection(emitterRecord, its, itsConnection, materialRecordConnection);

    if (dot(itsConnection.normal, -emitterRecordFromConnection.direction) < 0.0) {
        return float3(0.0);
    }

    //if the primary and secundary normal are not facing each other they cannot be connected
    /*
    if (dot(its.normal, itsConnection.normal) > 0.0) {
        return float3(0.0);
    }*/

    if (dot(emitterRecordFromConnection.direction, its.normal) < 1e-4f) {
        return float3(0.0);
    }

    // check if we hit a backface
    /*
    if (dot(materialRecordConnection.shadingFrame.n, -emitterRecordFromConnection.direction) < 0.0) {
        return float3(0.0);
    }
        */

    //normals are flipped on backfacing geometry resulting in light leaking through
    float3 radiance = getUnoccludedLightContribution(its, material, emitterRecordFromConnection);

    return radiance;
    //return radiance / lightSourcePdf(lightBuffer, lightVertex.lightIndex);
    
}

// this code sometimes still returns false posiitives iit is completly unclear why
bool isSurfacePointOccludedTest(in IntersectionRecord its, in IntersectionRecord itsConnection, EmitterRecord emitterRecord) {
    // 1. Use a robust offset to prevent self-intersection
    RayDesc shadowRay;
    // shadowRay.Origin = its.position + (its.normal * 0.001);
    shadowRay.Origin = its.position;
    shadowRay.Direction = normalize(emitterRecord.direction);
    shadowRay.TMin = 0.001;
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

bool isSurfacePointOccluded(in float3 startPos, in float3 endPos, in float3 normal) {
    RayDesc shadowRay;
    // Offset origin slightly along the normal to prevent self-intersection
    shadowRay.Origin = startPos + (normal * 0.001);

    float3 toTarget = endPos - startPos;
    float distance = length(toTarget);

    shadowRay.Direction = toTarget / distance;
    shadowRay.TMin = 0.0;
    shadowRay.TMax = distance - 0.001; // Stop just before target

    // Horizon check
    if (dot(shadowRay.Direction, normal) <= 0.0) return true;

    HitPayload payload;
    payload.hit = false;
    const uint rayFlags = RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH;
    TraceRay(topLevelAS, rayFlags, 0xff, 0, 0, 0, shadowRay, payload);

    return payload.hit;
}


float3 getOccludedLightContributionFromConnection(in IntersectionRecord its, in MaterialRecord material, in uint lightVertexIndex) {

    LightVertex lightVertex = lightVertexCache[lightVertexIndex];

    if (length(lightVertex.radiance) <= 0.0) {
        return float3(0.0);
    }

    IntersectionRecord itsConnection = getIntersectionRecordFromLightVertex(lightVertex, its);

    if (length(itsConnection.position - its.position) < 0.01) {
        return float3(0.0);
    }

    Material materialConnection = materials[itsConnection.materialIdx];
    MaterialRecord materialRecordConnection = getMaterialProperties(materialConnection, textures, itsConnection.uv, itsConnection.frame);

    EmitterRecord emitterRecord = getEmitterRecordFromLightVertex(lightVertex);
    EmitterRecord emitterRecordFromConnection = getEmitterRecordFromConnection(emitterRecord, its, itsConnection, materialRecordConnection);

    //check if we hit a backface
    if (dot(itsConnection.normal, -emitterRecordFromConnection.direction) < 0.0) {
        return float3(0.0);
    }

    if (isSurfacePointOccludedTest(its, itsConnection, emitterRecordFromConnection)) {
        return float3(0.0);
    }

    float3 radiance = getUnoccludedLightContribution(its, material, emitterRecordFromConnection);

    //almost certainly double count but lets go with
    //float cosAtEye = max(0.0, dot(its.normal, emitterRecordFromConnection.direction));
    //radiance *= cosAtEye;

    return radiance;
}

/*
float calculateCameraVertexAreaPdf(in IntersectionRecord itsCamera, float4x4 view, float4x4 viewInv, float4x4 proj, int2 launchSize) {
    // 1. Transform the world-space ray direction into view-space.
    // This allows us to find the angle of the ray relative to the camera's optical axis (Z-axis).
    // Note: Adjust the mul() order if your engine uses column-major matrices!
    float3 viewDir = mul(view, float4(itsCamera.direction, 0.0)).xyz;
    float cosThetaCam = abs(viewDir.z);

    if (cosThetaCam < 1e-5f) return 0.0f;

    // 2. Compute the solid angle PDF (p_omega) of the camera ray.
    // This calculates the angular tightness of a single pixel.
    float screenArea = float(launchSize.x * launchSize.y);
    float projFactors = proj[0][0] * proj[1][1]; // 1/tan(fovX/2) * 1/tan(fovY/2)

    // Derived from the Jacobian changing pixel area to view-space area to solid angle
    float p_omega = (screenArea * projFactors) / (4.0f * pow(cosThetaCam, 3.0f));

    // 3. Extract the camera's world-space position to calculate the distance.
    float3 cameraWorldPos = mul(viewInv, float4(0.0f, 0.0f, 0.0f, 1.0f)).xyz;

    float distance = length(itsCamera.position - cameraWorldPos);
    if (distance < 1e-5f) return 0.0f;

    // 4. Convert the solid angle PDF to a surface area PDF at the hit point z1.
    float cosTheta_z1 = saturate(dot(itsCamera.normal, -itsCamera.direction));

    float cameraVertexAreaPdf = p_omega * (cosTheta_z1 / (distance * distance));

    return cameraVertexAreaPdf;
}
    */

float3 getOccludedLightContributionFromConnectionTest(in IntersectionRecord itsCamera,
                                                      in MaterialRecord materialCamera,
                                                      in uint lightVertexIndex)
{
    LightVertex y1 = lightVertexCache[lightVertexIndex];
    if (length(y1.radiance) <= 0.0) return float3(0.0);

    // 1. Calculate Connection Geometry
    float3 dir_C2L = y1.hitPosition.xyz - itsCamera.position;
    float distance = length(dir_C2L);
    if (distance < 0.001) return float3(0.0);

    dir_C2L /= distance;
    float3 dir_L2C = -dir_C2L;
    float distSq = distance * distance;

    // 2. Visibility Check
    if (isSurfacePointOccluded(itsCamera.position, y1.hitPosition.xyz, itsCamera.normal)) {
        return float3(0.0);
    }

    // bsdf from z1 to y1
    BSDFQueryRecord bsdfRecCamera = constructBSDFRecord(itsCamera, materialCamera, dir_C2L, true);
    float3 bsdf_z1 = PrincipledEvalNoCosine(bsdfRecCamera, materialCamera);

    float3 dir_L2LightSource = y1.samplePosition.xyz - y1.hitPosition.xyz;
    float distanceL = length(dir_L2LightSource);
    if (distanceL < 0.001) return float3(0.0);

    dir_L2LightSource /= distanceL;
    float distSqL = distanceL * distanceL;

    //intersection record comming from z1
    IntersectionRecord itsLight;
    itsLight.position = y1.hitPosition.xyz;
    itsLight.normal = y1.normal.xyz;
    itsLight.tangent = y1.tangent.xyz;
    itsLight.frame = Frame(itsLight.normal, itsLight.tangent);
    itsLight.direction = dir_C2L;

    Material materialLight = materials[y1.materialIndex];
    MaterialRecord matRecLight = getMaterialProperties(materialLight, textures, y1.uv, itsLight.frame);

    //bsdf from y1 to light source
    BSDFQueryRecord bsdfRecLight = constructBSDFRecord(itsLight, matRecLight, dir_L2LightSource, true);
    float3 bsdf_y1 = PrincipledEvalNoCosine(bsdfRecLight, matRecLight);

    if (any(isnan(bsdf_z1)) || any(isnan(bsdf_y1))) return float3(0.0);

    // geometry term from y1 to light source
    // the cosin of the hit on the light surface is already backed into the radiance
    float cosinLtoLightSourceOnL = max(0.0, dot(dir_L2LightSource, itsLight.normal));
    float gLtoS = cosinLtoLightSourceOnL / distSqL;

    // geometry term from z1 to y1
    float cosinCtoLOnC = max(0.0, dot(dir_C2L, itsCamera.normal));
    float cosinCtoL = max(0.0, dot(dir_L2C, itsLight.normal));
    float gCtoL = cosinCtoL * cosinCtoLOnC / distSq;

    float3 throughput = (bsdf_z1 * bsdf_y1) * gCtoL * gLtoS;

    float3 finalRadiance = y1.radiance.xyz * throughput / y1.selectionPdf;

    return max(0.0, finalRadiance);
}

float3 getOccludedLightContributionFromConnectionExact(
    in IntersectionRecord itsCamera,
    in MaterialRecord materialCamera,
    in uint lightVertexIndex)
{
    LightVertex y1 = lightVertexCache[lightVertexIndex];
    if (length(y1.radiance) <= 0.0)
        return float3(0.0);

    float3 dir_C2L = y1.hitPosition.xyz - itsCamera.position;
    float dist_C = length(dir_C2L);
    if (dist_C < 0.01) // original uses 0.01
        return float3(0.0);
    dir_C2L /= dist_C;
    float3 dir_L2C = -dir_C2L;

    IntersectionRecord itsLight;
    itsLight.position = y1.hitPosition.xyz;
    itsLight.normal = y1.normal.xyz;
    itsLight.tangent = y1.tangent.xyz;
    itsLight.frame = Frame(itsLight.normal, itsLight.tangent);
    itsLight.direction = dir_C2L; // incoming direction from camera
    itsLight.materialIdx = y1.materialIndex;
    itsLight.uv = y1.uv;

    Material materialLight = materials[y1.materialIndex];
    MaterialRecord matRecLight = getMaterialProperties(materialLight, textures,
                                                       itsLight.uv, itsLight.frame);

    float3 dir_L2Source = y1.samplePosition.xyz - y1.hitPosition.xyz;
    float dist_L = length(dir_L2Source);
    if (dist_L < 0.001)
        return float3(0.0);
    dir_L2Source /= dist_L;
    float distSqL = max(dist_L * dist_L, 0.01);
    float3 sampleEmission_L = y1.radiance.xyz / distSqL;

    BSDFQueryRecord bsdfRecLight = constructBSDFRecord(itsLight, matRecLight,
                                                       dir_L2Source, true);
    float3 bsdf_y1 = PrincipledEval(bsdfRecLight, matRecLight);

    float3 bsdfEvalConnection = sampleEmission_L * bsdf_y1;

    float cosThetaLight = saturate(dot(itsLight.normal, dir_L2C));
    float distSqC = max(dist_C * dist_C, 0.01);
    float3 sampleEmission_C = bsdfEvalConnection * cosThetaLight / distSqC;

    if (cosThetaLight <= 0.0)
        return float3(0.0);

    RayDesc shadowRay;
    shadowRay.Origin = itsCamera.position;
    shadowRay.Direction = dir_C2L; // already normalized
    shadowRay.TMin = 0.001;
    shadowRay.TMax = dist_C - 0.001;

    if (dot(shadowRay.Direction, itsCamera.normal) < 1e-4f)
        return float3(0.0);

    HitPayload payload;
    payload.hit = false;
    const uint rayFlags = RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH;
    TraceRay(topLevelAS, rayFlags, 0xff, 0, 0, 0, shadowRay, payload);
    if (payload.hit)
        return float3(0.0);

    BSDFQueryRecord bsdfRecCamera = constructBSDFRecord(itsCamera, materialCamera,
                                                        dir_C2L, true);
    float3 bsdf_z1 = PrincipledEval(bsdfRecCamera, materialCamera);

    float3 radiance = bsdf_z1 * sampleEmission_C;

    if (any(isnan(radiance)) || any(isinf(radiance)))
        return float3(0.0);

    float3 finalRadiance = radiance / y1.selectionPdf;

    return max(0.0, finalRadiance);
}

/*

float3 getOccludedLightContributionFromConnectionTest(in IntersectionRecord itsCamera,
                                                      in MaterialRecord materialCamera,
                                                      in uint lightVertexIndex,
                                                      in float cameraVertexAreaPdf)
{
    LightVertex y1 = lightVertexCache[lightVertexIndex];
    if (length(y1.radiance) <= 0.0) return float3(0.0);

    // 1. Calculate Connection Geometry
    float3 dir_C2L = y1.hitPosition.xyz - itsCamera.position;
    float distance = length(dir_C2L);
    if (distance < 0.001) return float3(0.0);

    dir_C2L /= distance;
    float3 dir_L2C = -dir_C2L;
    float distSq = distance * distance;

    // 2. Visibility Check
    if (isSurfacePointOccluded(itsCamera.position, y1.hitPosition.xyz, itsCamera.normal)) {
        return float3(0.0);
    }

    // 3. Evaluate Camera BSDF (z1)
    BSDFQueryRecord bsdfRecCamera = constructBSDFRecord(itsCamera, materialCamera, dir_C2L, true);
    float3 bsdf_z1 = PrincipledEval(bsdfRecCamera, materialCamera);

    // 4. Evaluate Light Vertex BSDF (y1)
    float3 dir_L2LightSource = normalize(y1.samplePosition.xyz - y1.hitPosition.xyz);

    IntersectionRecord itsLight;
    itsLight.position = y1.hitPosition.xyz;
    itsLight.normal = y1.normal.xyz;
    itsLight.tangent = y1.tangent.xyz;
    itsLight.frame = Frame(itsLight.normal, itsLight.tangent);
    itsLight.direction = -dir_L2LightSource;

    Material materialLight = materials[y1.materialIndex];
    MaterialRecord matRecLight = getMaterialProperties(materialLight, textures, y1.uv, itsLight.frame);

    BSDFQueryRecord bsdfRecLight = constructBSDFRecord(itsLight, matRecLight, dir_L2C, true);
    float3 bsdf_y1 = PrincipledEval(bsdfRecLight, matRecLight);

    if (any(isnan(bsdf_z1)) || any(isnan(bsdf_y1))) return float3(0.0);

    // 5. Geometry Term
    float3 throughput = (bsdf_z1 * bsdf_y1) / distSq;

    // 6. Calculate Backward Area PDFs across the bridge for MIS
    float pdf_z1_to_y1_solidAngle = PrincipledPdf(bsdfRecCamera, materialCamera);
    float cosTheta_y1 = saturate(dot(itsLight.normal, dir_L2C));
    float backwardPdf_y1 = pdf_z1_to_y1_solidAngle * (cosTheta_y1 / distSq);

    float pdf_y1_to_z1_solidAngle = PrincipledPdf(bsdfRecLight, matRecLight);
    float cosTheta_z1 = saturate(dot(itsCamera.normal, dir_C2L));
    float backwardPdf_z1 = pdf_y1_to_z1_solidAngle * (cosTheta_z1 / distSq);

    // 7. Calculate MIS Weight
    float ratio_PT = backwardPdf_y1 / y1.selectionPdf;
    float ratio_LT = backwardPdf_z1 / cameraVertexAreaPdf;
    float misWeight = 1.0 / (ratio_PT + 1.0 + ratio_LT);

    // FIX 2: Do NOT divide by y1.selectionPdf here anymore!
    // y1.radiance already scales by the inverted emission PDF.
    float3 finalRadiance = y1.radiance.xyz * throughput * misWeight;

    return max(0.0, finalRadiance);
}
*/

EmitterRecord getEmitterRecordFromLightVertexNoPDF(in LightVertex lightVertex) {
    EmitterRecord emitterRecord;
    emitterRecord.direction = normalize(lightVertex.samplePosition.xyz - lightVertex.hitPosition.xyz);
    emitterRecord.distance = length(lightVertex.samplePosition.xyz - lightVertex.hitPosition.xyz);
    emitterRecord.intersection = lightVertex.hitPosition.xyz;

    float distSq = max(emitterRecord.distance * emitterRecord.distance, 0.01);

    LightSource light = lightBuffer[lightVertex.lightIndex];
    float cosThetaOnLight = dot(normalize(lightVertex.samplePosition - light.position), normalize(lightVertex.hitPosition - lightVertex.samplePosition));

    emitterRecord.sampleEmission = (lightVertex.radiance.xyz * cosThetaOnLight / distSq);


    return emitterRecord;
}

float3 getOccludedLightContributionFromConnectionNoPDF(in IntersectionRecord its, in MaterialRecord material, in uint lightVertexIndex) {

    LightVertex lightVertex = lightVertexCache[lightVertexIndex];

    if (length(lightVertex.radiance) <= 0.0) {
        return float3(0.0);
    }

    IntersectionRecord itsConnection = getIntersectionRecordFromLightVertex(lightVertex, its);

    if (length(itsConnection.position - its.position) < 0.01) {
        return float3(0.0);
    }

    // if the primary and secundary normal are not facing each other they cannot be connected
    /*
    if (dot(its.normal, itsConnection.normal) > 0.0) {
        return float3(0.0);
    }
        */

    Material materialConnection = materials[itsConnection.materialIdx];
    MaterialRecord materialRecordConnection = getMaterialProperties(materialConnection, textures, itsConnection.uv, itsConnection.frame);

    EmitterRecord emitterRecord = getEmitterRecordFromLightVertexNoPDF(lightVertex);
    EmitterRecord emitterRecordFromConnection = getEmitterRecordFromConnection(emitterRecord, its, itsConnection, materialRecordConnection);

    // check if we hit a backface
    if (dot(itsConnection.normal, -emitterRecordFromConnection.direction) < 0.0) {
        return float3(0.0);
    }

    /*
    if (dot(materialRecordConnection.shadingFrame.n, -emitterRecordFromConnection.direction) < 0.0) {
        return float3(0.0);
    }
        */

    if (isSurfacePointOccludedTest(its, itsConnection, emitterRecordFromConnection)) {
        return float3(0.0);
    }

    float3 radiance = getUnoccludedLightContribution(its, material, emitterRecordFromConnection);

    // return radiance / lightVertex.selectionPdf;

    return radiance;
}