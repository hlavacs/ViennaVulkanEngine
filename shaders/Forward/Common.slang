// Common.slang
module common;

//-----------------------------------------------------------------------

export public struct UniformBufferObjectTexture {
    public float4x4 model;
    public float4x4 modelInvTranspose;
    public float2 	uvscale;
};

//-----------------------------------------------------------------------

export public struct Color {
    public float4 ambient;
    public float4 diffuse;
    public float4 specular;
};

export public struct UniformBufferObjectColor {
    public float4x4 model;
    public float4x4 modelInvTranspose;
    public Color color;
};

//-----------------------------------------------------------------------

// x==1...point, x==2...directional, x==3...spotlight
export public struct Light {
    public float3 positionW;
    public float3 directionW;
    public float3 color;
    public float4 param;       // x = type, y = intensity, z = power, w = ambient
    public float3 attenuation; // x = constant, y = linear, z = quadratic
};

[[vk::constant_id(0)]] const int sizeLightArray = 10;

export public struct Camera {
    public float4x4 view;
    public float4x4 proj;
};

export public struct UniformBufferFrame {
    public Camera camera;
    public int3 numberLights; // x = point, y = directional, z = spotlight
    public Light lights[sizeLightArray];
};

//-----------------------------------------------------------------------

float3 calculatePointLight(Light light, float3 normal, float3 viewPos, float3 fragPos) {
	float3 result = float3(0.0);

	float3 ambient = light.param.w * light.color;
	float3 lightDir = normalize(light.positionW - fragPos);
	float3 viewDir = normalize(viewPos - fragPos);

	// Compute diffuse and specular components
	float diff = max(dot(normal, lightDir), 0.0);
	float3 reflectDir = reflect(-lightDir, normal);
	float spec = pow(max(dot(viewDir, reflectDir), 0.0), light.param.z);
	float3 diffuse = diff * light.color * light.param.y;
	float3 specular = spec * light.color * light.param.y;

	// Compute attenuation
	float distance = length(light.positionW - fragPos);
	float attenuation = 1.0 / (light.attenuation.x + light.attenuation.y * distance + light.attenuation.z * distance * distance);

	return attenuation * (ambient + diffuse + specular);

}

float3 calculateDirectionalLight(Light light, float3 normal, float3 viewPos, float3 fragPos) {
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

	return ambient + diffuse + specular;
}

float3 calculateSpotLight(Light light, float3 normal, float3 viewPos, float3 fragPos) {
	float3 result = float3(0.0);

	float3 ambient = light.param.w * light.color;
	float3 lightDir = normalize(light.positionW - fragPos);
	float3 viewDir = normalize(viewPos - fragPos);

	// Compute diffuse and specular components
	float diff = max(dot(normal, lightDir), 0.0);
	float3 reflectDir = reflect(-lightDir, normal);
	float spec = pow(max(dot(viewDir, reflectDir), 0.0), light.param.z);
	float3 diffuse = diff * light.color * light.param.y;
	float3 specular = spec * light.color * light.param.y;

	// Compute attenuation
	float distance = length(light.positionW - fragPos);
	float attenuation = 1.0 / (light.attenuation.x + light.attenuation.y * distance + light.attenuation.z * distance * distance);

	// Compute spotlight cone
	float theta = dot(lightDir, normalize(light.directionW));
	float epsilon = 0.9; // Adjust for spotlight cone
	float intensity = clamp((theta - epsilon) / (1.0 - epsilon), 0.0, 1.0);

	return attenuation * intensity * (ambient + diffuse + specular);
}

float3 calculateLighting(Light light[sizeLightArray], int3 numberLights, float3 normal, float3 viewPos, float3 fragPos) {
    float3 result = float3(0.0);

    /*float3 ambient = light.param.w * light.color;
    float3 lightDir = normalize(light.directionW);
    float3 viewDir = normalize(viewPos - fragPos);

    // Compute diffuse and specular components
    float diff = max(dot(normal, lightDir), 0.0);
    float3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), light.param.z);
    float3 diffuse = diff * light.color * light.param.y;
    float3 specular = spec * light.color * light.param.y;*/

	for( int i = 0; i < numberLights.x; i++ ) {// Point light
		int idx = i;
        //float distance = length(light[idx].positionW - fragPos);
        //float attenuation = 1.0 / (light[idx].attenuation.x + light[idx].attenuation.y * distance + light[idx].attenuation.z * distance * distance);
        //result += attenuation * (ambient + diffuse + specular);
		result += calculatePointLight(light[idx], normal, viewPos, fragPos);
    }

    for (int i = 0; i < numberLights.y; i++) { // Directional light
		int idx = numberLights.x + i;
        //result += ambient + diffuse + specular;
		result += calculateDirectionalLight(light[idx], normal, viewPos, fragPos);
    }

    for (int i = 0; i < numberLights.z; i++) { // Spot light
		int idx = numberLights.x + numberLights.y + i;
        //float theta = dot(lightDir, normalize(light[idx].positionW - fragPos));
        //float epsilon = 0.9; // Adjust for spotlight cone
        //float intensity = clamp((theta - epsilon) / (1.0 - epsilon), 0.0, 1.0);
        //result += intensity * (ambient + diffuse + specular);
		result += calculateSpotLight(light[idx], normal, viewPos, fragPos);
    }

    return result;
}

// Shadow map generation shaders
float calculateShadow(Sampler2D shadowMap, float3 lightSpacePos) {
    // Perspective divide for projection space
    float3 projCoords = lightSpacePos / lightSpacePos.z;
    projCoords = projCoords * 0.5 + 0.5; // Transform to [0, 1] range

    // Retrieve shadow map depth value
    float shadowDepth = shadowMap.Sample(projCoords.xy).r;

    // Compare and return shadow factor
    return (projCoords.z > shadowDepth + 0.005) ? 0.3 : 1.0; // 0.3 for shadowed, 1.0 for lit
}

float3 applyShadow(Light light, float3 lighting, float3 fragPos, float3 normal, Sampler2D shadowMap) {
    float3 lightSpacePos = float3(0.0); // Replace with actual transformation to light space
    float shadowFactor = calculateShadow(shadowMap, lightSpacePos);
    return shadowFactor * lighting;
}

