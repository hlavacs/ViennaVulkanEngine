
module Materials;
import Frame;

export public struct Material
{
    public float4 albedo;
    public int    albedoTextureIndex;
    public int    normalTextureIndex;
    public int    roughnessTextureIndex;
    public int    metalnessTextureIndex;
    public int    iorTextureIndex;
    public int    alphaTextureIndex;

    public float  roughness;
    public float  metalness;
    public float  ior;
    public float  alpha;
};

public struct MaterialRecord {
    public float3 albedo;
    public float3 normal;
    public float roughness;
    public float metalness;
    public float ior;
    public float alpha;

    public Frame shadingFrame;
}

Frame getShadingFrame(Frame geometryFrame, MaterialRecord material) {
    float3 shadingNormal = geometryFrame.toWorld(material.normal);
    Frame shadingFrame = Frame(shadingNormal);
    return shadingFrame;
}

public MaterialRecord getMaterialProperties(in Material mat, in Sampler2D textures[], in float2 texCoord, in Frame geometryFrame) {
    MaterialRecord record;

    record.albedo = getAlbedo(mat, textures, texCoord);
    record.normal = getNormal(mat, textures, texCoord);
    record.roughness = getRoughness(mat, textures, texCoord);
    record.metalness = getMetallic(mat, textures, texCoord);
    record.ior = getIOR(mat, textures, texCoord);
    record.alpha = getAlpha(mat, textures, texCoord);

    record.shadingFrame = getShadingFrame(geometryFrame, record);

    return record;
}

export public float3 getAlbedo(in Material mat, in Sampler2D textures[], in float2 texCoord){
    if(mat.albedoTextureIndex == -1){
        return mat.albedo.xyz;
    }else{
        return textures[mat.albedoTextureIndex].Sample(texCoord).xyz;
    }
}

float3 SRGBToLinear(float3 c)
{
    return lerp(
        c / 12.92,
        pow((c + 0.055) / 1.055, 2.4),
        step(0.04045, c)
    );
}

float3 LinearToSRGB(float3 c)
{
    return lerp(
        c * 12.92,
        1.055 * pow(c, 1.0 / 2.4) - 0.055,
        step(0.0031308, c)
    );
}

export public float3 getNormal(in Material mat, in Sampler2D textures[], in float2 texCoord) {
    if (mat.normalTextureIndex == -1) {
        return float3(0.0,0.0,1.0);
    } else {
        // float3 mappedNormal = LinearToSRGB(textures[mat.normalTextureIndex].Sample(texCoord).xyz);
        float3 mappedNormal = textures[mat.normalTextureIndex].Sample(texCoord).xyz;
        float3 normal;

        //normal is in srgb
        normal = (mappedNormal * 2.0) - 1.0;
        //normal.xy = (mappedNormal.xy - float2(0.5, 0.5)) * 2.0;
        //normal.z = sqrt(saturate(1.0 - normal.x * normal.x - normal.y * normal.y));
        return normalize(normal);
    }
}

export public float getAlpha(in Material mat, in Sampler2D textures[], in float2 texCoord){
    if(mat.alphaTextureIndex == -1){
        return mat.alpha;
    }else{
        return textures[mat.alphaTextureIndex].Sample(texCoord).x;
    }
}

export public float getRoughness(in Material mat, in Sampler2D textures[], in float2 texCoord){
    if(mat.roughnessTextureIndex == -1){
        return mat.roughness;
    }else{
        return textures[mat.roughnessTextureIndex].Sample(texCoord).x;
    }
}

export public float getMetallic(in Material mat, in Sampler2D textures[], in float2 texCoord){
    if(mat.metalnessTextureIndex == -1){
        return mat.metalness;
    }else{
        return textures[mat.metalnessTextureIndex].Sample(texCoord).x;
    }
}

export public float getIOR(in Material mat, in Sampler2D textures[], in float2 texCoord){
    if(mat.iorTextureIndex == -1){
        return mat.ior;
    }else{
        return textures[mat.iorTextureIndex].Sample(texCoord).x;
    }
}