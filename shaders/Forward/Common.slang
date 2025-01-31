// Common.slang
module common;

export public struct Color {
    public float4 ambient;
    public float4 diffuse;
    public float4 specular;
};

// Example data structures
export public struct Material {
    public float3 albedo;
    public float roughness;
    public float metallic;
};

//x==1...point, x==2...directional, x==3...spotlight
export public struct Light {
    public float3 positionW;
    public float3 directionW;
    public float3 color;
	public float4 param; // x = type, y = intensity, z = power, w = ambient
    public float3 attenuation; // x = constant, y = linear, z = quadratic
};

export public struct Camera {
    public float4x4 view;
    public float4x4 proj;
};

export public struct UniformBufferObject {
    public float4x4 model;
    public float4x4 modelInvTranspose;
    public Color    color;
};

export public struct UniformBufferFrame {
    public Camera camera;
};

export public struct UniformBufferLights {
    public Light lights[5];
};

float3 calculateLighting(Light light, float3 normal, float3 viewPos, float3 fragPos) {
    float3 result = float3(0.0);

    float3 ambient = light.param.w * light.color;
    float3 lightDir = normalize(light.directionW);
    float3 viewDir = normalize(viewPos - fragPos);

    // Compute diffuse and specular components
    float diff = max(dot(normal, lightDir), 0.0);
    float3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), light.param.z);
    float3 diffuse = diff * light.color * light.param.y;
    float3 specular = spec * light.color * light.param.y;

    if (light.param.x == 1.0) { // Point light
        float distance = length(light.positionW - fragPos);
        float attenuation = 1.0 / (light.attenuation.x + light.attenuation.y * distance + light.attenuation.z * distance * distance);
        result = attenuation * (ambient + diffuse + specular);
    } else if (light.param.x == 2.0) { // Directional light
        result = ambient + diffuse + specular;
    } else if (light.param.x == 3.0) { // Spotlight
        float theta = dot(lightDir, normalize(light.positionW - fragPos));
        float epsilon = 0.9; // Adjust for spotlight cone
        float intensity = clamp((theta - epsilon) / (1.0 - epsilon), 0.0, 1.0);
        result = intensity * (ambient + diffuse + specular);
    }

    return result;
}
