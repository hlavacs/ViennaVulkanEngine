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

//-----------------------------------------------------------------------

export public float3 calculatePointLight(Light light, float3 nW, float3 viewPosW, float3 fragPosW) {
	float3 result = float3(0.0);

	float3 ambient = light.param.param.w * light.param.color;
	float3 lightDir = normalize(light.positionW - fragPosW);
	float3 viewDir = normalize(viewPosW - fragPosW);
	float3 normalW = normalize(nW);

	// Compute diffuse and specular components
	float diff = max(dot(normalW, lightDir), 0.0);

	float3 reflectDir = reflect(-lightDir, -normalW);
	float spec = pow(max(dot(viewDir, reflectDir), 0.0), light.param.param.z);

	float3 diffuse = diff * light.param.color * light.param.param.y;
	float3 specular = spec * light.param.color * light.param.param.y;

	// Compute attenuation
	float distance = length(light.positionW - fragPosW);
	float attenuation = 1.0 / (light.param.attenuation.x + light.param.attenuation.y * distance + light.param.attenuation.z * distance * distance);

	return attenuation * (ambient + diffuse + specular);
}

export public float3 calculateDirectionalLight(Light light, float3 nW, float3 viewPosW, float3 fragPosW) {
	float3 result = float3(0.0);

	float3 ambient = light.param.param.w * light.param.color;
	float3 lightDir = -normalize(light.directionW); //points towards the light
	float3 viewDir = normalize(viewPosW - fragPosW);
	float3 normalW = normalize(nW);

	// Compute diffuse and specular components
	float diff = max(dot(normalW, lightDir), 0.0);

	float3 reflectDir = reflect(-lightDir, -normalW);
	float spec = pow(max(dot(viewDir, reflectDir), 0.0), light.param.param.z);

	float3 diffuse = diff * light.param.color * light.param.param.y;
	float3 specular = spec * light.param.color * light.param.param.y;

	return ambient + diffuse + specular;
}

export public float3 calculateSpotLight(Light light, float3 nW, float3 viewPosW, float3 fragPosW) {
	float3 result = float3(0);

	float3 ambient = light.param.param.w * light.param.color;
	float3 lightDir = normalize(light.positionW - fragPosW);
	float3 viewDir = normalize(viewPosW - fragPosW);
	float3 normalW = normalize(nW);

	// Compute diffuse and specular components
	float diff = max(dot(normalW, lightDir), 0.0);
	
	float3 reflectDir = reflect(-lightDir, -normalW);
	float spec = pow(max(dot(viewDir, reflectDir), 0.0), light.param.param.z);

	float3 diffuse = diff * light.param.color * light.param.param.y;
	float3 specular = spec * light.param.color * light.param.param.y;

	// Compute attenuation
	float distance = length(light.positionW - fragPosW);
	float attenuation = 1.0 / (light.param.attenuation.x + light.param.attenuation.y * distance + light.param.attenuation.z * distance * distance);

	// Compute spotlight cone
	float intensity = pow(max(dot(lightDir, -normalize(light.directionW)), 0.0), light.param.param.z);

	return attenuation * intensity * (ambient + diffuse + specular);
}


export public float3 calculateLighting(StructuredBuffer<Light> light, int3 numberLights, float3 normalW, float3 viewPosW, float3 fragPosW) {
    float3 result = float3(0.0);

    [unroll]
    for (int i = 0; i < numberLights.x + numberLights.y + numberLights.z; ++i) {
        if (light[i].param.param.x == 1) result += calculatePointLight(light[i], normalW, viewPosW, fragPosW);
        if (light[i].param.param.x == 2) result += calculateDirectionalLight(light[i], normalW, viewPosW, fragPosW);
        if (light[i].param.param.x == 3) result += calculateSpotLight(light[i], normalW, viewPosW, fragPosW);
    }

    return result;
}
