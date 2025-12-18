
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

export public float3 getAlbedo(in Material mat, in Sampler2D textures[], in float2 texCoord){
    if(mat.albedoTextureIndex == -1){
        return mat.albedo.xyz;
    }else{
        return textures[mat.albedoTextureIndex].Sample(texCoord).xyz;
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