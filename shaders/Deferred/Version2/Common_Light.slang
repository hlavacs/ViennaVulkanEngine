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

// https://learnopengl.com/PBR/Lighting

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

// https://learnopengl.com/PBR/Lighting

export public float3 accumulatePointLights(StructuredBuffer<Light> light, int start, int end, float3 normalW, float3 viewPosW, float3 posW, float3 albedo, float metallic, float roughness) {

    float3 N = normalize(normalW);
    float3 V = normalize(viewPosW - posW);
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);

    float3 Lo = float3(0.0);
    for (int i = start; i < end; ++i) {
        float3 L = normalize(light[i].positionW - posW);
        float3 H = normalize(V + L);

        float distance = length(light[i].positionW - posW);
        float attenuation = 1.0 / (light[i].param.attenuation.x + light[i].param.attenuation.y * distance + light[i].param.attenuation.z * distance * distance);
        float3 radiance = light[i].param.color * light[i].param.param.y * attenuation;

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
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }

    return Lo;
}

export public float3 accumulateDirectLights(StructuredBuffer<Light> light, int start, int end, float3 normalW, float3 viewPosW, float3 posW, float3 albedo, float metallic, float roughness) {

    float3 N = normalize(normalW);
    float3 V = normalize(viewPosW - posW);
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);

    float3 Lo = float3(0.0);
    for (int i = start; i < end; ++i) {
        float3 L = -normalize(light[i].directionW);
        float3 H = normalize(V + L);

        float3 radiance = light[i].param.color * light[i].param.param.y;

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
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }
    
    return Lo;
}

export public float3 accumulateSpotLights(StructuredBuffer<Light> light, int start, int end, float3 normalW, float3 viewPosW, float3 posW, float3 albedo, float metallic, float roughness) {

    float3 N = normalize(normalW);
    float3 V = normalize(viewPosW - posW);
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);

    float3 Lo = float3(0.0);
    for (int i = start; i < end; ++i) {
        float3 L = normalize(light[i].positionW - posW);
        float3 H = normalize(V + L);

        float distance = length(light[i].positionW - posW);
        float intensity = pow(max(dot(L, -normalize(light[i].directionW)), 0.0), light[i].param.param.z);
        float attenuation = 1.0 / (light[i].param.attenuation.x + intensity * distance + light[i].param.attenuation.z * distance * distance);
        float3 radiance = light[i].param.color * light[i].param.param.y * attenuation;

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
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }

    return Lo;
}

export public float4 calculateLighting(StructuredBuffer<Light> light, int3 numberLights, float3 normalW, float3 viewPosW, float3 posW, float3 albedo, float metallic, float roughness) {
    float3 Lo = float3(0.0);

    int start = 0;
    int end = numberLights.x;
    Lo += accumulatePointLights(light, start, end, normalW, viewPosW, posW, albedo, metallic, roughness);
    start = end;
    end += numberLights.y;
    Lo += accumulateDirectLights(light, start, end, normalW, viewPosW, posW, albedo, metallic, roughness);
    start = end;
    end += numberLights.z;
    Lo += accumulateSpotLights(light, start, end, normalW, viewPosW, posW, albedo, metallic, roughness);

    float3 ambient = float(0.01) * albedo /* ao */;
    float3 color = ambient + Lo;

    return float4(color, 1.0);
}
