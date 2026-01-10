// Helper functions for lighting calculations.

// Constants used for lighting.
const vec3 dielectricSpecular = vec3(0.04, 0.04, 0.04);
const vec3 black = vec3(0, 0, 0);
const vec3 AmbientLight = vec3(0.4, 0.4, 0.25) * 10.0;
const float SurfaceOffset = 0.01;

struct ShadingResult {
	vec3 Diffuse;
	vec3 Specular;
};

// Basic Lambertian diffuse
// Implementation from Lambert's Photometria https://archive.org/details/lambertsphotome00lambgoog
// See also [1], Equation 1
vec3 diffuse(vec3 diffuseColor)
{
	return diffuseColor / M_PI;
}

// The following equation models the Fresnel reflectance term of the spec equation (aka F())
// Implementation of fresnel from [4], Equation 15
vec3 specularReflection(vec3 reflectance0, vec3 reflectance90, float VdotH)
{
	return reflectance0 + (reflectance90 - reflectance0) * pow(clamp(1.0 - VdotH, 0.0, 1.0), 5.0);
}

// This calculates the specular geometric attenuation (aka G()),
// where rougher material will reflect less light back to the viewer.
// This implementation is based on [1] Equation 4, and we adopt their modifications to
// alphaRoughness as input as originally proposed in [2].
float geometricOcclusion(float NdotL, float NdotV, float alphaRoughness)
{
	float roughnessSq = alphaRoughness * alphaRoughness;
	float attenuationL = 2.0 * NdotL / (NdotL + sqrt(roughnessSq + (1.0 - roughnessSq) * (NdotL * NdotL)));
	float attenuationV = 2.0 * NdotV / (NdotV + sqrt(roughnessSq + (1.0 - roughnessSq) * (NdotV * NdotV)));
	return attenuationL * attenuationV;
}

// The following equation(s) model the distribution of microfacet normals across the area being drawn (aka D())
// Implementation from "Average Irregularity Representation of a Roughened Surface for Ray Reflection" by T. S. Trowbridge, and K. P. Reitz
// Follows the distribution function recommended in the SIGGRAPH 2013 course notes from EPIC Games [1], Equation 3.
float microfacetDistribution(float NdotH, float alphaRoughness)
{
	float roughnessSq = alphaRoughness * alphaRoughness;
	float f = (NdotH * roughnessSq - NdotH) * NdotH + 1.0;
	return roughnessSq / (M_PI * f * f);
}


// Calculates light attenuation and a new randomized light direction based on the light type and radius.
float GetLightAttenuationAndLightVec(vec3 position, RendererLight light, vec4 rngValues,  out vec4 lightVec) {
    float attenuation = 0.0;

    if(light.Type == LIGHT_TYPE_POINT || light.Type == LIGHT_TYPE_SPOT) {
		lightVec.xyz = light.Position - position;
		lightVec.w = length(lightVec.xyz);
		lightVec.xyz = lightVec.xyz / lightVec.w;
	} else { // LIGHT_TYPE_DIRECTIONAL
		lightVec.xyz = -light.Direction;
		lightVec.w = 1000.0;
	}

	vec2 samplePos = BlueNoiseInDisk[int(rngValues.z * 64)];
	vec2 diskPoint;
	diskPoint.x = samplePos.x * rngValues.x - samplePos.y * rngValues.y;
	diskPoint.y = samplePos.x * rngValues.y + samplePos.y * rngValues.x;
	diskPoint *= light.Radius;

	vec3 lightNormal = lightVec.xyz;
	vec3 lightTangent;
	if (abs(lightNormal.y) > 0.99) {
	   lightTangent = normalize(cross(lightNormal, vec3(0.0f, 0.0f, 1.0f)));
	} else{
	   lightTangent = normalize(cross(lightNormal, vec3(0.0f, 1.0f, 0.0f)));
	}
	vec3 lightBitangent = normalize(cross(lightTangent, lightNormal));
	lightVec.xyz = normalize(lightNormal + diskPoint.x * lightTangent + diskPoint.y * lightBitangent);

    if(light.Type == LIGHT_TYPE_POINT || light.Type == LIGHT_TYPE_SPOT) {
	   	if(lightVec.w < light.Range) {
	     	float a = (lightVec.w / light.Range);
	     	attenuation = max(min( 1.0 - a * a * a * a, 1.0 ), 0.0 ) / (lightVec.w * lightVec.w);

	     	if(light.Type == LIGHT_TYPE_SPOT) {
	     		float cd = dot(-light.Direction, lightVec.xyz);
				float angularAttenuation = clamp(cd * light.AngleScale + light.AngleOffset, 0.0, 1.0);
				attenuation *= angularAttenuation * angularAttenuation;
	     	}
	   	}
    } else { // LIGHT_TYPE_DIRECTIONAL
 		attenuation = 1.0;
    }

    return attenuation;
}

vec3 GetLightRadiance(SurfacePoint point, vec3 viewDirection, vec4 rngValues, RendererLight light, bool shadow) {
	vec3 surfacePos = point.Position + point.Normal * SurfaceOffset;
    float alphaRoughness = point.Roughness * point.Roughness;

    vec3 diffuseColor = mix(point.Albedo.rgb * (1.0 - dielectricSpecular.r), black, point.Metalness);
    vec3 specularColor = mix(dielectricSpecular, point.Albedo.rgb, point.Metalness);

	float reflectance = max(max(specularColor.r, specularColor.g), specularColor.b);
	float reflectance90 = clamp(reflectance * 25.0, 0.0, 1.0);
	vec3 specularEnvironmentR0 = specularColor.rgb;
	vec3 specularEnvironmentR90 = vec3(1.0, 1.0, 1.0) * reflectance90;

	vec4 lightVec;
	float attenuation = GetLightAttenuationAndLightVec(point.Position, light, rngValues, lightVec);
	if(attenuation > 0.0) {
		if(shadow && CastVisRay(surfacePos + lightVec.xyz * lightVec.w, surfacePos)) {
			attenuation = 0.0;
		}
	}
	if(attenuation <= 0.0) {
		return vec3(0.0, 0.0, 0.0);
	}
	vec3 halfVec = normalize(lightVec.xyz + viewDirection);

	float NdotL = clamp(dot(point.Normal, lightVec.xyz), 0.001, 1.0);
	float NdotV = clamp(abs(dot(point.Normal, viewDirection)), 0.001, 1.0);
	float NdotH = clamp(dot(point.Normal, halfVec), 0.0, 1.0);
	float LdotH = clamp(dot(lightVec.xyz, halfVec), 0.0, 1.0);
	float VdotH = clamp(dot(viewDirection, halfVec), 0.0, 1.0);

	// Calculate the shading terms for the microfacet specular shading model
	vec3 F = specularReflection(specularEnvironmentR0, specularEnvironmentR90, VdotH);
	float G = geometricOcclusion(NdotL, NdotV, alphaRoughness);
	float D = microfacetDistribution(NdotH, alphaRoughness);

	// Calculation of analytical lighting contribution
	// Obtain final intensity as reflectance (BRDF) scaled by the energy of the light (cosine law)
	vec3 lighting = (NdotL * attenuation * light.Intensity) * light.Color;
	return  ((1.0 - F) * lighting) + ((lighting * F * G) * D / (4.0 * NdotL * NdotV));
}

vec3 GetApproximateLightRadiance(SurfacePoint point, vec3 viewDirection, vec4 rngValues, RendererLight light, bool shadow) {
	vec3 surfacePos = point.Position + point.Normal * SurfaceOffset;
    
	vec4 lightVec;
	float attenuation = GetLightAttenuationAndLightVec(point.Position, light, rngValues, lightVec);
	if(attenuation > 0.0) {
		if(shadow && CastVisRay(surfacePos + lightVec.xyz * lightVec.w, surfacePos)) {
			attenuation = 0.0;
		}
	}
	if(attenuation <= 0.0) {
		return vec3(0.0, 0.0, 0.0);
	}

	float NdotL = clamp(dot(point.Normal, lightVec.xyz), 0.001, 1.0);

	// Calculation of approximate analytical lighting contribution
	vec3 lighting = (NdotL * attenuation * light.Intensity) * light.Color;
	return lighting;
}

float GetLightVisibility(SurfacePoint point, vec4 rngValues, RendererLight light) {
	vec3 surfacePos = point.Position + point.Normal * SurfaceOffset;
	vec4 lightVec;
	float attenuation = GetLightAttenuationAndLightVec(point.Position, light, rngValues, lightVec);
	if(attenuation > 0.0) {
		if(CastVisRay(surfacePos + lightVec.xyz * lightVec.w, surfacePos)) {
			attenuation = 0.0;
		}
	}
	return attenuation;
}

// Simplified shadinng used for reflections in non path traced rendering.
ShadingResult ShadePointSimple(SurfacePoint point, vec4 rngValues) 
{
    vec3 surfacePos = point.Position + point.Normal * SurfaceOffset;
    vec3 diffuseColor = mix(point.Albedo.rgb * (1.0 - dielectricSpecular.r), black, point.Metalness);

    ShadingResult result;
    result.Diffuse = vec3(0, 0, 0);
    result.Specular = vec3(0, 0, 0);
    for(int i = 0; i < LightCount; ++i) {
    	RendererLight light = GetLight(i);
    	vec4 lightVec;
    	float attenuation = GetLightAttenuationAndLightVec(point.Position, light, rngValues, lightVec);
    	if(attenuation > 0.0) {
		    if(!CastVisRay(surfacePos + lightVec.xyz * lightVec.w, surfacePos)) {
				float NdotL = clamp(dot(point.Normal, lightVec.xyz), 0.001, 1.0);
				result.Diffuse += light.Color * diffuseColor * (NdotL * attenuation * light.Intensity / M_PI);
	 		}
 		}
    }
	
    return result;
}



// Shade a point without pathtracing.
ShadingResult ShadePoint(SurfacePoint point, vec3 viewDirection, vec4 rngValues) 
{
    vec3 surfacePos = point.Position + point.Normal * SurfaceOffset;
    float alphaRoughness = point.Roughness * point.Roughness;

    vec3 diffuseColor = mix(point.Albedo.rgb * (1.0 - dielectricSpecular.r), black, point.Metalness);
    vec3 specularColor = mix(dielectricSpecular, point.Albedo.rgb, point.Metalness);

	float reflectance = max(max(specularColor.r, specularColor.g), specularColor.b);
	float reflectance90 = clamp(reflectance * 25.0, 0.0, 1.0);
	vec3 specularEnvironmentR0 = specularColor.rgb;
	vec3 specularEnvironmentR90 = vec3(1.0, 1.0, 1.0) * reflectance90;

    ShadingResult result;
    result.Diffuse = vec3(0, 0, 0);
    result.Specular = vec3(0, 0, 0);

    for(int i = 0; i < LightCount; ++i) {
    	RendererLight light = GetLight(i);
    	vec4 lightVec;
    	float attenuation = GetLightAttenuationAndLightVec(point.Position, light, rngValues, lightVec);
    	if(attenuation > 0.0) {
		    if(CastVisRay(surfacePos + lightVec.xyz * lightVec.w, surfacePos)) {
				attenuation = 0.0;
	 		}
 		}

	    vec3 halfVec = normalize(lightVec.xyz + viewDirection);

		float NdotL = clamp(dot(point.Normal, lightVec.xyz), 0.001, 1.0);
		float NdotV = clamp(abs(dot(point.Normal, viewDirection)), 0.001, 1.0);
		float NdotH = clamp(dot(point.Normal, halfVec), 0.0, 1.0);
		float LdotH = clamp(dot(lightVec.xyz, halfVec), 0.0, 1.0);
		float VdotH = clamp(dot(viewDirection, halfVec), 0.0, 1.0);

		// Calculate the shading terms for the microfacet specular shading model
		vec3 F = specularReflection(specularEnvironmentR0, specularEnvironmentR90, VdotH);
		float G = geometricOcclusion(NdotL, NdotV, alphaRoughness);
		float D = microfacetDistribution(NdotH, alphaRoughness);

		// Calculation of analytical lighting contribution
		// Obtain final intensity as reflectance (BRDF) scaled by the energy of the light (cosine law)
		vec3 lighting = (NdotL * attenuation * light.Intensity) * light.Color;
		result.Diffuse += (1.0 - F) * lighting; //(diffuseColor / M_PI) * 
		result.Specular += (lighting * F * G) * D / (4.0 * NdotL * NdotV);
    }

    // Add some reflection, this is not 100% pbr.
    SurfacePoint nextPoint = point;
    vec3 lastViewDir = viewDirection;
    for(int i = 0; i < RENDERING_MAX_RECURSIONS; ++i) {
	    if(nextPoint.Roughness < 0.5 || nextPoint.Metalness > 0.5) {
			vec2 samplePos = BlueNoiseInDisk[i];
            vec2 diskPoint;
			diskPoint.x = samplePos.x * rngValues.x - samplePos.y * rngValues.y;
			diskPoint.y = samplePos.x * rngValues.y + samplePos.y * rngValues.x;
			diskPoint *= nextPoint.Roughness;

			vec3 reflectionVec = normalize(reflect(-lastViewDir, nextPoint.Normal));
			vec3 reflectionTangent = normalize(cross(reflectionVec, vec3(0.0f, 1.0f, 0.0f)));
			vec3 reflectionBitangent = normalize(cross(reflectionTangent, reflectionVec));

			vec3 glossyReflectionVec = normalize(reflectionVec + diskPoint.x * reflectionTangent + diskPoint.y * reflectionBitangent);

			if(CastSurfaceRay(surfacePos, surfacePos + glossyReflectionVec * 200.0, nextPoint)) {
				lastViewDir = -glossyReflectionVec;
				surfacePos = nextPoint.Position + nextPoint.Normal * SurfaceOffset;
				ShadingResult reflection = ShadePointSimple(nextPoint, rngValues); 
				result.Specular += reflection.Diffuse + reflection.Specular;
			} else {
				break;
			}
		} 
    }

    return result;
}

// Shading calculation used for pathtracing points.
// This could be a lot simplified by using a proper path tracer...
ShadingResult ShadePointSimplePT(SurfacePoint point, vec3 viewDirection, vec4 rngValues) 
{
    vec3 surfacePos = point.Position + point.Normal * SurfaceOffset;
    float alphaRoughness = point.Roughness * point.Roughness;

    vec3 diffuseColor = mix(point.Albedo.rgb * (1.0 - dielectricSpecular.r), black, point.Metalness);
    vec3 specularColor = mix(dielectricSpecular, point.Albedo.rgb, point.Metalness);

	float reflectance = max(max(specularColor.r, specularColor.g), specularColor.b);
	float reflectance90 = clamp(reflectance * 25.0, 0.0, 1.0);
	vec3 specularEnvironmentR0 = specularColor.rgb;
	vec3 specularEnvironmentR90 = vec3(1.0, 1.0, 1.0) * reflectance90;

    ShadingResult result;
    result.Diffuse = vec3(0, 0, 0);
    result.Specular = vec3(0, 0, 0);

    for(int i = 0; i < LightCount; ++i) {
    	RendererLight light = GetLight(i);
    	vec4 lightVec;
    	float attenuation = GetLightAttenuationAndLightVec(point.Position, light, rngValues, lightVec);
    	if(attenuation > 0.0) {
		    if(CastVisRay(surfacePos + lightVec.xyz * lightVec.w, surfacePos)) {
				attenuation = 0.0;
	 		}
 		}

		if (attenuation > 0) {
			vec3 halfVec = normalize(lightVec.xyz + viewDirection);

			float NdotL = clamp(dot(point.Normal, lightVec.xyz), 0.001, 1.0);
			float NdotV = clamp(abs(dot(point.Normal, viewDirection)), 0.001, 1.0);
			float NdotH = clamp(dot(point.Normal, halfVec), 0.0, 1.0);
			float LdotH = clamp(dot(lightVec.xyz, halfVec), 0.0, 1.0);
			float VdotH = clamp(dot(viewDirection, halfVec), 0.0, 1.0);

			// Calculate the shading terms for the microfacet specular shading model
			vec3 F = specularReflection(specularEnvironmentR0, specularEnvironmentR90, VdotH);
			float G = geometricOcclusion(NdotL, NdotV, alphaRoughness);
			float D = microfacetDistribution(NdotH, alphaRoughness);

			// Calculation of analytical lighting contribution
			// Obtain final intensity as reflectance (BRDF) scaled by the energy of the light (cosine law)
			vec3 lighting = (NdotL * attenuation * light.Intensity) * light.Color;
			result.Diffuse += (diffuseColor / M_PI) * (1.0 - F) * lighting; //
			result.Specular += (lighting * F * G) * D / (4.0 * NdotL * NdotV);
		}
    }
	
    return result;
}

// Shades a point using path tracing.
// This is currently not a very well implemented path tracer...
ShadingResult ShadePointPT(SurfacePoint point, vec3 viewDirection, vec4 rngValues) 
{
    vec3 surfacePos = point.Position + point.Normal * SurfaceOffset;

    float seed = rngValues.z * GOLDENRATIO_CONJUGATE;
    int sampleCount = 1;
    SurfacePoint currentPoint = point;
    SurfacePoint nextPoint = point;
    vec3 lastRayDir = viewDirection;
	
	ShadingResult result = ShadePointSimplePT(nextPoint, lastRayDir, rngValues);
	result.Diffuse *= M_PI;

    for(int k = 0; k < RENDERING_MAX_SAMPLES; ++k) {
        for(int i = 0; i < RENDERING_MAX_RECURSIONS; ++i) {
			vec3 newRay = cosWeightedRandomHemisphereDirection(nextPoint.Normal, seed); 
			bool nextIsSpecular;
			if(hash1(seed) > nextPoint.Roughness) {
				newRay = mix(reflect(-lastRayDir, nextPoint.Normal), newRay, nextPoint.Roughness); 
				nextIsSpecular = true;
			} else {
				newRay = cosWeightedRandomHemisphereDirection(nextPoint.Normal, seed); 
				nextIsSpecular = false;
			}

            if(CastSurfaceRay(surfacePos, surfacePos + newRay * 200.0, nextPoint)) {
				ShadingResult temp = ShadePointSimplePT(nextPoint, lastRayDir, rngValues);
				if(nextIsSpecular) {
					result.Specular += temp.Diffuse + temp.Specular;
				} else {
					result.Diffuse += (currentPoint.Albedo.rgb / M_PI) * (temp.Diffuse + temp.Specular);
				}

                lastRayDir = -newRay;
                surfacePos = nextPoint.Position + nextPoint.Normal * SurfaceOffset;

                currentPoint = nextPoint;
            } else {
                break;
            }
        }
		
        sampleCount++;
    }

    result.Diffuse /= float(sampleCount);
    result.Specular /= float(sampleCount);
    
    return result;
}

