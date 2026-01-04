import Warp;
import Random;
import Frame;

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


EmitterRecord sampleSphereLight(in LightSource light, in float3 origin, inout Sampler sampler) {
    EmitterRecord record;
    Frame frame = Frame(normalize(origin - light.position.xyz));
    float3 hemisphereCoords = squareToUniformHemisphere(sampler.UniformFloat2());

    record.intersection = light.position.xyz + frame.toWorld(hemisphereCoords) * light.radius;
    record.distance = length(record.intersection - origin);
    record.direction = normalize(record.intersection - origin);
    // /light.pdf????????????
    record.sampleEmission = (light.emission.xyz) / (record.distance * record.distance);
    return record;
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

EmitterRecord sampleDiskLight(in LightSource light, in float3 origin, inout Sampler sampler) {
    EmitterRecord record;
    Frame frame = Frame(normalize(light.direction.xyz));
    float3 diskCoords = float3(squareToUniformDisk(sampler.UniformFloat2()), 0.0);

    record.intersection = light.position.xyz + frame.toWorld(diskCoords) * light.radius;
    record.distance = length(record.intersection - origin);
    record.direction = normalize(record.intersection - origin);
    record.sampleEmission = (light.emission.xyz) / dot(light.direction.xyz, record.direction) * (record.distance * record.distance);
    return record;
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