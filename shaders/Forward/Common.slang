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

export public struct Light {
    public float3 position;
    public float3 color;
    public float intensity;
};

export public struct UniformBufferObject {
    public float4x4 model;
    public float4x4 modelInvTranspose;
    public Color    color;
};

export public struct UniformBufferCamera {
    public float4x4 view;
    public float4x4 proj;
};

// Example helper function
float3 calculateLighting(Light light, float3 normal, float3 viewDir) {
    float3 lightDir = normalize(light.position - viewDir);
    float3 halfDir = normalize(lightDir + viewDir);
    float ndotl = max(dot(normal, lightDir), 0.0);
    float ndoth = max(dot(normal, halfDir), 0.0);
    return light.color * light.intensity * ndotl;
}

