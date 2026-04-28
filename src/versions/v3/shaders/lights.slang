/// @file Shared lighting data declarations used by v3 shaders.
static const uint kMaxLights = 8;

static const uint kLightTypeDirectional = 0;
static const uint kLightTypePoint = 1;
static const uint kLightTypeSpot = 2;

struct LightData
{
    uint type;
    float intensity;
    float innerConeCos;
    float outerConeCos;
    float3 position;
    float range;
    float3 direction;
    float padding0;
    float3 color;
    float padding1;
};

struct LightingConstants
{
    uint lightCount;
    float3 ambientColor;
    LightData lights[kMaxLights];
};

float computeDistanceAttenuation(float3 lightVector, float range)
{
    float distanceSquared = dot(lightVector, lightVector);
    if (distanceSquared <= 1e-6)
    {
        return 1.0;
    }

    float distance = sqrt(distanceSquared);
    float attenuation = 1.0 / max(1.0, distanceSquared * 0.04);
    if (range > 0.0)
    {
        float normalizedDistance = saturate(distance / range);
        float rangeFactor = saturate(1.0 - normalizedDistance * normalizedDistance);
        attenuation *= rangeFactor * rangeFactor;
    }

    return attenuation;
}

float computeSpotAttenuation(float3 lightDirection, float3 spotDirection, float innerConeCos, float outerConeCos)
{
    float cosTheta = dot(lightDirection, normalize(-spotDirection));
    float coneRange = max(innerConeCos - outerConeCos, 1e-4);
    return saturate((cosTheta - outerConeCos) / coneRange);
}

float3 evaluateLight(in LightData light, float3 worldPosition, float3 worldNormal)
{
    float3 lightDirection = 0.0;
    float attenuation = 1.0;

    if (light.type == kLightTypeDirectional)
    {
        lightDirection = normalize(light.direction);
    }
    else
    {
        float3 lightVector = light.position - worldPosition;
        float distanceSquared = dot(lightVector, lightVector);
        if (distanceSquared <= 1e-6)
        {
            return 0.0;
        }

        lightDirection = lightVector * rsqrt(distanceSquared);
        attenuation = computeDistanceAttenuation(lightVector, light.range);

        if (light.type == kLightTypeSpot)
        {
            attenuation *= computeSpotAttenuation(
                lightDirection,
                light.direction,
                light.innerConeCos,
                light.outerConeCos);
        }
    }

    float lambert = saturate(dot(worldNormal, lightDirection));
    return light.color * (light.intensity * attenuation * lambert);
}
