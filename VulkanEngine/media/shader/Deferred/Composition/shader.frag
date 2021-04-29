#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "../../common_defines.glsl"
#include "../../light.glsl"

layout(location = 0) in vec2 inUV;

layout (location = 0) out vec4 outColor;


layout(set = 0, binding = 0) uniform cameraUBO_t {
    cameraData_t data;
} cameraUBO;

layout(set = 1, binding = 0) uniform lightUBO_t {
    lightData_t data;
} lightUBO;

layout(set = 2, binding = 0) uniform sampler2D shadowMap[NUM_SHADOW_CASCADE];

layout (set = 3, binding = 0) uniform sampler2D samplerPosition;
layout (set = 3, binding = 1) uniform sampler2D samplerNormal;
layout (set = 3, binding = 2) uniform sampler2D samplerAlbedo;

void main()
{


	// Read G-Buffer values from previous sub pass
	vec3 fragPos = texture(samplerPosition, inUV).rgb;
	vec3 normal = texture(samplerNormal, inUV).rgb;
	vec3 albedo = texture(samplerAlbedo, inUV).rgb;

	//parameters
	vec3 camPosW   = cameraUBO.data.camModel[3].xyz;
	int  lightType  = lightUBO.data.itype[0];
	vec3 lightPosW = lightUBO.data.lightModel[3].xyz;
	vec3 lightDirW = normalize(lightUBO.data.lightModel[2].xyz);
	float nfac = dot(normal, -lightDirW)<0? 0.5:1;
	vec4 lightParam = lightUBO.data.param;

	vec3 ambcol  = lightUBO.data.col_ambient.xyz;
	vec3 diffcol = lightUBO.data.col_diffuse.xyz;
	vec3 speccol = lightUBO.data.col_specular.xyz;

	int sIdx = 0;
	cameraData_t s = lightUBO.data.shadowCameras[0];
	float shadowFactor = 1.0;
	vec3 result = albedo*ambcol;

	//skyplane and color
	if (length(normal)<0.01)
	{
		if(lightType == LIGHT_AMBIENT)
		{
			result = albedo;
		}
		else
		{
			result = vec3(0.0);
		}
	}
	else
	{
		result = ambcol * albedo;
		if (lightType == LIGHT_DIR) {
			sIdx = shadowIdxDirectional(cameraUBO.data.param,
										vec4(fragPos,1.0),
										lightUBO.data.shadowCameras[0].param[3],
										lightUBO.data.shadowCameras[1].param[3],
										lightUBO.data.shadowCameras[2].param[3]);

			s = lightUBO.data.shadowCameras[sIdx];
			shadowFactor = shadowFunc(fragPos, s.camView, s.camProj, shadowMap[sIdx]);
			shadowFactor = 1.0;
			result +=   dirlight(lightType, camPosW,
			lightDirW, lightParam, shadowFactor,
			ambcol, diffcol, speccol,
			fragPos, normal, albedo);
		}


		if (lightType == LIGHT_POINT) {

			sIdx = shadowIdxPoint(lightPosW, fragPos);
			s = lightUBO.data.shadowCameras[sIdx];
			shadowFactor = shadowFunc(fragPos, s.camView, s.camProj, shadowMap[sIdx]);

			result +=   pointlight(lightType, camPosW,
			lightPosW, lightParam, shadowFactor,
			ambcol, diffcol, speccol,
			fragPos, normal, albedo);
		}

		if (lightType == LIGHT_SPOT) {

			shadowFactor = shadowFunc(fragPos, s.camView, s.camProj, shadowMap[sIdx]);

			result +=  spotlight(lightType, camPosW,
			lightPosW, lightDirW, lightParam, shadowFactor,
			ambcol, diffcol, speccol,
			fragPos, normal, albedo);
		}
	}
	outColor = vec4(result, 1.0);
}