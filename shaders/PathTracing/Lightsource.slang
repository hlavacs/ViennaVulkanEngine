import Warp;
import Random;
import Frame;
import Defines;

public struct LightSource
{
    public float4 position;
    public float4 emission;
    public float4 direction;
    public float radius;
    public float pdf;
    public float accumulativeSampleWeight;
    public uint lightType;
};

public struct EmitterRecord {
    public float3 sampleEmission;
    public float distance;
    public float3 intersection;
    public float3 direction;
}

public struct RandomEmitterRecord {
    public EmitterRecord record;
    public float pdf;
}

/*
EmitterRecord sampleSphereLight(in LightSource light, in float3 origin, inout Sampler sampler) {
    EmitterRecord record;
    Frame frame = Frame(normalize(origin - light.position.xyz));
    float3 hemisphereCoords = squareToUniformHemisphere(sampler.UniformFloat2());

    record.intersection = light.position.xyz + frame.toWorld(hemisphereCoords) * light.radius;
    record.distance = length(record.intersection - origin);
    record.direction = normalize(record.intersection - origin);
    // /light.pdf????????????

    // dot product needed between sphere normal and record.direction

    //somethings wrong with the normal
    float3 sphereNormal = normalize(frame.toWorld(hemisphereCoords));

    // record.sampleEmission = (light.emission.xyz) / dot(sphereNormal, -record.direction) * (record.distance * record.distance);
    record.sampleEmission = (light.emission.xyz) * dot(sphereNormal, -record.direction) / (record.distance * record.distance) * squareToUniformHemispherePdf(hemisphereCoords);

    //record.sampleEmission = (light.emission.xyz) / (record.distance * record.distance);
    return record;
}
    */

// uses solid angle sampling to imporve noise levels
EmitterRecord sampleSphereLight(in LightSource light,
                                in float3 origin,
                                inout Sampler sampler)
{
    EmitterRecord rec;

    float3 C = light.position.xyz;
    float3 P = origin;

    float3 wc = C - P;
    float d = length(wc);
    wc /= d;

    float r = light.radius;

    float sinThetaMax = r / d;
    float cosThetaMax = sqrt(max(0.0, 1.0 - sinThetaMax * sinThetaMax));

    // Sample cosine-weighted spherical cap
    float2 u = sampler.UniformFloat2();

    float cosTheta = lerp(cosThetaMax, 1.0, sqrt(u.x));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    float phi = 2.0 * M_PI * u.y;

    float3 localDir = float3(
        cos(phi) * sinTheta,
        sin(phi) * sinTheta,
        cosTheta
    );

    Frame frame = Frame(wc);
    float3 wi = frame.toWorld(localDir);

    // Ray–sphere intersection (guaranteed hit)
    float3 oc = P - C;
    float b = dot(oc, wi);
    float c = dot(oc, oc) - r * r;
    float h = b * b - c;
    float t = -b - sqrt(h);

    rec.intersection = P + wi * t;
    rec.distance = t;
    rec.direction = wi;

    // Final estimator for total emitted power Φ
    rec.sampleEmission = light.emission.xyz / (4.0 * M_PI * rec.distance * rec.distance);

    return rec;
}

/*
EmitterRecord sampleSphereLight(in LightSource light, in float3 origin, inout Sampler sampler) {
    EmitterRecord record;

    float3 sphereCoords = squareToUniformSphere(sampler.UniformFloat2());

    record.intersection = light.position.xyz + sphereCoords * light.radius;
    record.distance = length(record.intersection - origin);
    record.direction = normalize(record.intersection - origin);
    // /light.pdf????????????
    record.sampleEmission = (light.emission.xyz) / (record.distance * record.distance);
    return record;
}
*/

/*
EmitterRecord sampleDiskLight(in LightSource light, in float3 origin, inout Sampler sampler) {
    EmitterRecord record;
    Frame frame = Frame(normalize(light.direction.xyz));
    float3 diskCoords = float3(squareToUniformDisk(sampler.UniformFloat2()), 0.0);

    record.intersection = light.position.xyz + frame.toWorld(diskCoords) * light.radius;
    record.distance = length(record.intersection - origin);
    record.direction = normalize(record.intersection - origin);
    record.sampleEmission = (light.emission.xyz) * dot(light.direction.xyz, -record.direction) / (record.distance * record.distance) * squareToUniformDiskPdf(diskCoords.xy);
    return record;
}
    */

EmitterRecord sampleDiskLight(in LightSource light, in float3 origin, inout Sampler sampler)
{
    EmitterRecord rec;

    float3 P = origin;
    float3 C = light.position.xyz;
    float3 N = normalize(light.direction.xyz);
    float r = light.radius;

    // Direction from shading point to disk center
    float3 wc = C - P;
    float d = length(wc);
    wc /= d;

    // Compute the cone subtended by the disk
    float sinThetaMax = r / d;
    float cosThetaMax = sqrt(max(0.0, 1.0 - sinThetaMax * sinThetaMax));

    // Sample a direction inside the solid-angle cone
    float2 u = sampler.UniformFloat2();
    float cosTheta = lerp(cosThetaMax, 1.0, sqrt(u.x));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    float phi = 2.0 * M_PI * u.y;

    // Direction in local frame
    float3 localDir = float3(
        cos(phi) * sinTheta,
        sin(phi) * sinTheta,
        cosTheta
    );

    // Transform to world space
    Frame frame = Frame(wc);
    float3 wi = frame.toWorld(localDir);

    // Intersect ray with disk plane (guaranteed hit because we sampled the cone)
    float t = dot(C - P, N) / dot(wi, N);
    float3 hitPoint = P + wi * t;

    rec.intersection = hitPoint;
    rec.distance = length(hitPoint - P);
    rec.direction = wi;

    // Total emitted power Φ, energy-conserving
    rec.sampleEmission = light.emission.xyz / (4.0 * M_PI * rec.distance * rec.distance);

    return rec;
}

EmitterRecord sampleLightSource(in LightSource light, in float3 origin, inout Sampler sampler) {
    EmitterRecord record;

    if (light.lightType == 0) {
        record = sampleSphereLight(light, origin, sampler);
    } else if (light.lightType == 1) {
        record = sampleDiskLight(light, origin, sampler);
    }
    return record;
}

// returns the light influence from a randomly selected light.
public EmitterRecord sampleRandomLightSource(in StructuredBuffer<LightSource> lightSources, in float3 origin, inout Sampler sampler) {
    float pdf = 0.0;
    LightSource light = chooseRandomLightSource(lightSources, sampler, pdf);
    EmitterRecord record = sampleLightSource(light, origin, sampler);

    record.sampleEmission = record.sampleEmission / pdf;


    return record;
}

LightSource chooseRandomLightSource(in StructuredBuffer<LightSource> lightSources, inout Sampler sampler, inout float pdf) {
    float rand = sampler.UniformFloat();
    uint index = 0;
    LightSource chosenLight;
    while (true) {
        LightSource light = lightSources[index];
        if (light.accumulativeSampleWeight >= rand) {
            chosenLight = light;
            if (index == 0) {
                pdf = light.accumulativeSampleWeight;
            } else {
                LightSource previousLight = lightSources[index - 1];
                pdf = light.accumulativeSampleWeight - previousLight.accumulativeSampleWeight;
            }
            break;
        }
        index++;
    }
    return chosenLight;
}