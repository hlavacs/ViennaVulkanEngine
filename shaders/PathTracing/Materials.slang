
module Materials;
import Frame;

#ifdef RAY_TRACING
    #define SAMPLE_2D(tex, uv) tex.SampleLevel(uv, 0.0)
#else
    #define SAMPLE_2D(tex, uv) tex.Sample(uv)
#endif

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

    public float pad1;
    public float pad2;
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
    // Inputs: geometry frame and material record. Output: shading frame from normal map.
    float3 shadingNormal = geometryFrame.toWorld(material.normal);
    Frame shadingFrame = Frame(shadingNormal);
    return shadingFrame;
}

public MaterialRecord getMaterialProperties(in Material mat, in Sampler2D textures[], in float2 texCoord, in Frame geometryFrame) {
    // Inputs: material, texture array, UV, geometry frame. Output: resolved material record.
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
    // Inputs: material, textures, UV. Output: base color.
    if(mat.albedoTextureIndex == -1){
        return mat.albedo.xyz;
    } else {
        // for rasterizer implicit sampling should be used: .Sample(texCoordz
        return SAMPLE_2D(textures[mat.albedoTextureIndex], texCoord).xyz;   
    }
}

export public float3 getNormal(in Material mat, in Sampler2D textures[], in float2 texCoord) {
    // Inputs: material, textures, UV. Output: tangent-space normal.
    if (mat.normalTextureIndex == -1) {
        return float3(0.0,0.0,1.0);
    } else {
        float3 mappedNormal;
        mappedNormal = SAMPLE_2D(textures[mat.normalTextureIndex], texCoord).xyz; 
        float3 normal = (mappedNormal * 2.0) - 1.0;
        return normalize(normal);
    }
}

export public float getAlpha(in Material mat, in Sampler2D textures[], in float2 texCoord){
    // Inputs: material, textures, UV. Output: alpha.
    if(mat.alphaTextureIndex == -1){
        return mat.alpha;
    } else {
        return SAMPLE_2D(textures[mat.alphaTextureIndex], texCoord).x;
    }
}

export public float getRoughness(in Material mat, in Sampler2D textures[], in float2 texCoord){
    // Inputs: material, textures, UV. Output: roughness.
    if(mat.roughnessTextureIndex == -1){
        return mat.roughness;
    } else {
        return SAMPLE_2D(textures[mat.roughnessTextureIndex], texCoord).x;      
    }
}

export public float getMetallic(in Material mat, in Sampler2D textures[], in float2 texCoord){
    // Inputs: material, textures, UV. Output: metalness.
    if(mat.metalnessTextureIndex == -1){
        return mat.metalness;
    } else {
        return SAMPLE_2D(textures[mat.metalnessTextureIndex], texCoord).x;
    }
}

export public float getIOR(in Material mat, in Sampler2D textures[], in float2 texCoord){
    // Inputs: material, textures, UV. Output: IOR.
    if (mat.iorTextureIndex == -1) {
        return mat.ior;
    } else {
        return SAMPLE_2D(textures[mat.iorTextureIndex], texCoord).x;       
    }
}
