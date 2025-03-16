// Common.slang
module common;

//-----------------------------------------------------------------------

export public struct Camera {
    public float4x4 view;
    public float4x4 proj;
    public float3 positionW;
};

//param.x==1...point, param.x==2...directional, param.x==3...spotlight
export public struct LightParams {
	public float3 color; 
	public float4 param; //x=type, y=intensity, z=power, w=ambient
	public float3 attenuation; //x=constant, y=linear, z=quadratic
};

// x==1...point, x==2...directional, x==3...spotlight
export public struct Light {
    public float3 		positionW;
    public float3 		directionW;
    public LightParams 	params;
    public float4x4 	lightSpaceMatrix;
};

export public struct UniformBufferFrame {
    public Camera camera;
    public uint3 numberLights; // x = point, y = directional, z = spotlight
};

//-----------------------------------------------------------------------

export public struct UniformBufferObject {
    public float4x4 model;
    public float4x4 modelInvTranspose;
};

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

//[[vk::constant_id(0)]] export public uniform const int MAX_LIGHTS = 10;
export public uniform const int MAX_LIGHTS = 10;
export public uniform const int MAX_SHADOW_MAPS = 60;

__include "Common_Shadow.slang";
__include "Common_Light.slang";

[shader("fragment")]
float4 dummyShader(float3 positionW: POSITION)
{
    return float4(0, 0, 0, 0);
}
