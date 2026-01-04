
module Materials;

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
}

public MaterialRecord getMaterialProperties(in Material mat, in Sampler2D textures[], in float2 texCoord) {
    MaterialRecord record;

    record.albedo = getAlbedo(mat, textures, texCoord);
    record.normal = getNormal(mat, textures, texCoord);
    record.roughness = getRoughness(mat, textures, texCoord);
    record.metalness = getMetallic(mat, textures, texCoord);
    record.ior = getIOR(mat, textures, texCoord);
    record.alpha = getAlpha(mat, textures, texCoord);

    return record;
}

export public float3 getAlbedo(in Material mat, in Sampler2D textures[], in float2 texCoord){
    if(mat.albedoTextureIndex == -1){
        return mat.albedo.xyz;
    }else{
        return textures[mat.albedoTextureIndex].Sample(texCoord).xyz;
    }
}

export public float3 getNormal(in Material mat, in Sampler2D textures[], in float2 texCoord) {
    if (mat.normalTextureIndex == -1) {
        return float3(0.0,0.0,1.0);
    } else {
        return textures[mat.normalTextureIndex].Sample(texCoord).xyz;
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