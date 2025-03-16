implementing Common;

//-----------------------------------------------------------------------

export public float3 calculatePointLight(Light light, float3 nW, float3 viewPosW, float3 fragPosW) {
	float3 result = float3(0.0);

	float3 ambient = light.params.param.w * light.params.color;
	float3 lightDir = normalize(light.positionW - fragPosW);
	float3 viewDir = normalize(viewPosW - fragPosW);
	float3 normalW = normalize(nW);

	// Compute diffuse and specular components
	float diff = max(dot(normalW, lightDir), 0.0);

	float3 reflectDir = reflect(-lightDir, -normalW);
	float spec = pow(max(dot(viewDir, reflectDir), 0.0), light.params.param.z);

	float3 diffuse = diff * light.params.color * light.params.param.y;
	float3 specular = spec * light.params.color * light.params.param.y;

	// Compute attenuation
	float distance = length(light.positionW - fragPosW);
	float attenuation = 1.0 / (light.params.attenuation.x + light.params.attenuation.y * distance + light.params.attenuation.z * distance * distance);

	return attenuation * (ambient + diffuse + specular);
}

export public float3 calculateDirectionalLight(Light light, float3 nW, float3 viewPosW, float3 fragPosW) {
	float3 result = float3(0.0);

	float3 ambient = light.params.param.w * light.params.color;
	float3 lightDir = -normalize(light.directionW); //points towards the light
	float3 viewDir = normalize(viewPosW - fragPosW);
	float3 normalW = normalize(nW);

	// Compute diffuse and specular components
	float diff = max(dot(normalW, lightDir), 0.0);

	float3 reflectDir = reflect(-lightDir, -normalW);
	float spec = pow(max(dot(viewDir, reflectDir), 0.0), light.params.param.z);

	float3 diffuse = diff * light.params.color * light.params.param.y;
	float3 specular = spec * light.params.color * light.params.param.y;

	return ambient + diffuse + specular;
}

export public float3 calculateSpotLight(Light light, float3 nW, float3 viewPosW, float3 fragPosW) {
	float3 result = float3(0);

	float3 ambient = light.params.param.w * light.params.color;
	float3 lightDir = normalize(light.positionW - fragPosW);
	float3 viewDir = normalize(viewPosW - fragPosW);
	float3 normalW = normalize(nW);

	// Compute diffuse and specular components
	float diff = max(dot(normalW, lightDir), 0.0);
	
	float3 reflectDir = reflect(-lightDir, -normalW);
	float spec = pow(max(dot(viewDir, reflectDir), 0.0), light.params.param.z);

	float3 diffuse = diff * light.params.color * light.params.param.y;
	float3 specular = spec * light.params.color * light.params.param.y;

	// Compute attenuation
	float distance = length(light.positionW - fragPosW);
	float attenuation = 1.0 / (light.params.attenuation.x + light.params.attenuation.y * distance + light.params.attenuation.z * distance * distance);

	// Compute spotlight cone
	float intensity = pow(max(dot(lightDir, -normalize(light.directionW)), 0.0), light.params.param.z);

	return attenuation * intensity * (ambient + diffuse + specular);
}

export public float3 calculateLighting(Light light[], int3 numberLights, float3 normalW, float3 viewPosW, float3 fragPosW) {
    float3 result = float3(0.0);

	for( int i = 0; i < numberLights.x; i++ ) {// Point light
		int idx = i;
		result += calculatePointLight(light[idx], normalW, viewPosW, fragPosW);
    }

    for (int i = 0; i < numberLights.y; i++) { // Directional light
		int idx = numberLights.x + i;
		result += calculateDirectionalLight(light[idx], normalW, viewPosW, fragPosW);
    }

    for (int i = 0; i < numberLights.z; i++) { // Spot light
		int idx = numberLights.x + numberLights.y + i;
		result += calculateSpotLight(light[idx], normalW, viewPosW, fragPosW);
    }

    return result;
}

