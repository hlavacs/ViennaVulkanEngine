// IBL variant of Common_Light.slang for gaussian demos
// Adds irradiance cubemap sampling for ambient lighting from gaussian environments
implementing Common;

// param.x==0...no light, param.x==1...point, param.x==2...directional, param.x==3...spotlight
export public struct LightParams {
    public float3 color;
    float pad1;
    public float4 param;       // x=type, y=intensity, z=power, w=ambient
    public float3 attenuation; // x=constant, y=linear, z=quadratic
    float pad2;
};

// x==1...point, x==2...directional, x==3...spotlight
export public struct Light {
    public float3 		positionW;
	float pad1;
    public float3 		directionW;
	float pad2;
    public LightParams 	param;
};

// Used in Deferred Renderer to reconstruct positionW from depth
export public struct PushConstantLight {
    public float4x4 invViewProj;
}

static const float PI = 3.14159265359;

//-----------------------------------------------------------------------

// PBR shader code adapted from LearnOpenGL (https://learnopengl.com/PBR/Lighting)
// Original author: Joey de Vries - https://twitter.com/JoeyDeVriez
// used under CC BY-NC 4.0 - https://creativecommons.org/licenses/by-nc/4.0/

float3 fresnelSchlick(float cosTheta, float3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float DistributionGGX(float3 N, float3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

export public float3 accumulatePointLights(StructuredBuffer<Light> lights, int start, int end, float3 normalW, float3 viewPosW, float3 posW, float3 albedo, float metallic, float roughness, SamplerCubeArrayShadow cubeArrayShadow, int32_t enableShadow) {

    float3 N = normalize(normalW);
    float3 V = normalize(viewPosW - posW);
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);

    float3 Lo = float3(0.0);
    for (int i = start; i < end; ++i) {
        float sh = enableShadow == 0 ? 1 : calcPointShadow(posW, lights[i].positionW, cubeArrayShadow, i);
        if (sh == 0) continue;

        float3 L = normalize(lights[i].positionW - posW);
        float3 H = normalize(V + L);

        float distance = length(lights[i].positionW - posW);
        float attenuation = 1.0 / (lights[i].param.attenuation.x + lights[i].param.attenuation.y * distance + lights[i].param.attenuation.z * distance * distance);
        float3 radiance = lights[i].param.color * lights[i].param.param.y * attenuation;

        float NDF = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(N, V, L, roughness);
        float3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

        float3 kS = F;
        float3 kD = float3(1.0) - kS;
        kD *= 1.0 - metallic;

        float3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        float3 specular = numerator / denominator;

        float NdotL = max(dot(N, L), 0.0);

        Lo += (kD * albedo / PI + specular) * radiance * NdotL * sh;
    }

    return Lo;
}

export public float3 accumulateDirectLights(StructuredBuffer<Light> lights, int start, int end, float3 normalW, float3 viewPosW, float3 posW, float3 albedo, float metallic, float roughness, Sampler2DArrayShadow shadow2DArray, StructuredBuffer<float4x4> lsm, uint lsmIndex, int32_t enableShadow) {

    float3 N = normalize(normalW);
    float3 V = normalize(viewPosW - posW);
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);

    float3 Lo = float3(0.0);
    uint arrayIndex = lsmIndex;
    for (int i = start; i < end; ++i) {
        float sh = enableShadow == 0 ? 1 : calcDirectShadow(posW, lights[i].positionW, shadow2DArray, arrayIndex++, lsm);
        if (sh == 0) continue;

        float3 L = -normalize(lights[i].directionW);
        float3 H = normalize(V + L);

        float3 radiance = lights[i].param.color * lights[i].param.param.y;

        float NDF = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(N, V, L, roughness);
        float3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

        float3 kS = F;
        float3 kD = float3(1.0) - kS;
        kD *= 1.0 - metallic;

        float3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        float3 specular = numerator / denominator;

        float NdotL = max(dot(N, L), 0.0);

        Lo += (kD * albedo / PI + specular) * radiance * NdotL * sh;
    }

    return Lo;
}

export public float3 accumulateSpotLights(StructuredBuffer<Light> lights, int start, int end, float3 normalW, float3 viewPosW, float3 posW, float3 albedo, float metallic, float roughness, Sampler2DArrayShadow shadow2DArray, StructuredBuffer<float4x4> lsm, uint lsmIndex, int32_t enableShadow) {

    float3 N = normalize(normalW);
    float3 V = normalize(viewPosW - posW);
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);

    uint arrayIndex = lsmIndex;
    float3 Lo = float3(0.0);
    for (int i = start; i < end; ++i) {
        float sh = enableShadow == 0 ? 1 : calcSpotShadow(posW, lights[i].positionW, shadow2DArray, arrayIndex++, lsm);
        if (sh == 0) continue;

        float3 L = normalize(lights[i].positionW - posW);
        float3 H = normalize(V + L);

        float distance = length(lights[i].positionW - posW);
        float intensity = pow(max(dot(L, -normalize(lights[i].directionW)), 0.0), lights[i].param.param.z);
        // float attenuation = 1.0 / (lights[i].param.attenuation.x + intensity * distance + lights[i].param.attenuation.z * distance * distance);
        float attenuation = 1.0 / (lights[i].param.attenuation.x + lights[i].param.attenuation.y * distance + lights[i].param.attenuation.z * distance * distance);
        float3 radiance = lights[i].param.color * lights[i].param.param.y * attenuation * intensity;

        float NDF = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(N, V, L, roughness);
        float3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

        float3 kS = F;
        float3 kD = float3(1.0) - kS;
        kD *= 1.0 - metallic;

        float3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        float3 specular = numerator / denominator;

        float NdotL = max(dot(N, L), 0.0);

        Lo += (kD * albedo / PI + specular) * radiance * NdotL * sh;
    }

    return Lo;
}

// IBL variant: uses irradiance cubemap for ambient lighting instead of hardcoded value
export public float4 calculateLightingIBL(StructuredBuffer<Light> lights, int3 numberLights, float3 normalW, float3 viewPosW, float3 posW, float3 albedo, float metallic, float roughness, SamplerCubeArrayShadow cubeArrayShadow, Sampler2DArrayShadow shadow2DArray, StructuredBuffer<float4x4> lsm, int32_t enableShadow, SamplerCube irradianceCubemap) {
    float3 Lo = float3(0.0);

    uint start = 0;
    uint end = numberLights.x;
    Lo += accumulatePointLights(lights, start, end, normalW, viewPosW, posW, albedo, metallic, roughness, cubeArrayShadow, enableShadow);
    start = end;
    end += numberLights.y;
    Lo += accumulateDirectLights(lights, start, end, normalW, viewPosW, posW, albedo, metallic, roughness, shadow2DArray, lsm, 0, enableShadow);
    start = end;
    end += numberLights.z;
    Lo += accumulateSpotLights(lights, start, end, normalW, viewPosW, posW, albedo, metallic, roughness, shadow2DArray, lsm, numberLights.y, enableShadow);

    // Ambient lighting from irradiance cubemap (IBL from gaussian environment)
    float3 N = normalize(normalW);
    float3 irradiance = irradianceCubemap.Sample(N).rgb;
    float intensity = 1.0;
    float3 ambient = irradiance * albedo * intensity;
    float3 color = ambient + Lo;

    return float4(color, 1.0);
}
