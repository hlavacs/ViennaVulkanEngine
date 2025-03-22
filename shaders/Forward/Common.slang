// Common.slang
module common;

//-----------------------------------------------------------------------

export public struct Camera {
    public float4x4 view;
    public float4x4 proj;
    public float3 positionW;
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

export public uniform const int MAX_ARRAY_SIZE = 16;

__include "Common_Shadow.slang";
__include "Common_Light.slang";

[shader("fragment")]
float4 dummyShader(float3 positionW: POSITION)
{
    return float4(0, 0, 0, 0);
}
