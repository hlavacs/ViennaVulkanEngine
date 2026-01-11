import Defines;
import Warp;
import Materials;
import Random;
import Surface;
import Frame;

public struct BSDFQueryRecord {
    public float3 geometryNormal; 

    public float3 wiWorld;
    public float3 woWorld;

    public float3 wiShading;
    public float3 woShading;

    public bool wmSet;
    public float3 wm;
    public float eta;

    //this is not pretty replace it later.
    public bool isShadowQuery;
}

enum LightInteraction : int
{
    DIFFUSE = 0,
    SPECULAR = 1,
    METALLIC = 2,
    ALPHA = 3
};

float convertIORLevel(float iorLevel) {
    return 1.0 + iorLevel;
}

public BSDFQueryRecord constructBSDFRecord(in IntersectionRecord its, in MaterialRecord material, in bool isShadowQuery) {
    BSDFQueryRecord record;
    record.wiShading = material.shadingFrame.toLocal(-its.direction);
    record.wiWorld = -its.direction;
    record.geometryNormal = its.normal;
    record.wmSet = false;
    record.isShadowQuery = isShadowQuery;
    return record;
}

public BSDFQueryRecord constructBSDFRecord(in IntersectionRecord its, in MaterialRecord material, float3 outDirection, in bool isShadowQuery) {
    BSDFQueryRecord record;
    record.wiShading = material.shadingFrame.toLocal(-its.direction);
    record.wiWorld = -its.direction;
    record.woShading = material.shadingFrame.toLocal(outDirection);
    record.woWorld = outDirection;
    record.geometryNormal = its.normal;
    record.wmSet = false;
    record.isShadowQuery = isShadowQuery;
    return record;
}

float fresnel(in float cosTheta_i, in float eta) {
    float sin2Theta_i = 1 - pow(cosTheta_i, 2.0);
    float sin2Theta_t = sin2Theta_i / pow(eta, 2.0);
    float cosTheta_t = sqrt(1 - sin2Theta_t);

    float r_parl = (eta * cosTheta_i - cosTheta_t) /
                   (eta * cosTheta_i + cosTheta_t);
    float r_perp = (cosTheta_i - eta * cosTheta_t) /
                   (cosTheta_i + eta * cosTheta_t);
    return (pow(r_parl, 2.0) + pow(r_perp, 2.0)) / 2;
}

public float3 glossyEval(in BSDFQueryRecord record, in MaterialRecord material, bool isMetal) {

    float3 color;

    if (isMetal) {
        color = material.albedo;
    } else {
        color = float3(1.0, 1.0, 1.0);
    }

    if (material.roughness < 0.01) {
        if (record.isShadowQuery) {
            return float3(0.0, 0.0, 0.0);
        }
        return color; 
    }

    float cosWiG = dot(record.geometryNormal, record.wiWorld);
    if (cosWiG <= 0) return float3(0);
    float cosWoG = dot(record.geometryNormal, record.woWorld);
    if (cosWoG <= 0) return float3(0);

    // actully eta = m_intIOR / m_extIOR, but m_extIOR simplified to 1.0 for now

    //set wm if not yet set
    if (!record.wmSet) {
        record.wm = record.woShading + record.wiShading;
        record.wm = normalize(record.wm);
    }
    //F is maybe wrong here!!!!!!!!!!
    // https://www.youtube.com/watch?v=gya7x9H3mV0


    float alpha = pow(material.roughness, 2.0);

    return color * D(record.wm, alpha) * G(record.woShading, record.wiShading, alpha) / ((4 * record.wiShading.z * record.woShading.z)) * record.woShading.z;

    //return color * D(record.wm, alpha) * G(record.woShading, record.wiShading, alpha) * (1.0 - F) / ((4 * record.wiShading.z * record.woShading.z)) * record.woShading.z;
}

public float glossyPdf(in BSDFQueryRecord record, in MaterialRecord material) {

    if (material.roughness < 0.01) {
        return 0.0f;
    }

    if (!record.wmSet) {
        record.wm = record.woShading + record.wiShading;
        record.wm = normalize(record.wm);
    }

    float cosWiG = dot(record.geometryNormal, record.wiWorld);
    if (cosWiG <= 0) return 0.0;
    float cosWoG = dot(record.geometryNormal, record.woWorld);
    if (cosWoG <= 0) return 0.0;

    return squareToGGXPDF(record.woShading, record.wm, material.roughness) / (4 * abs(dot(record.woShading, record.wm)));
}

public float3 sampleGlossy(inout BSDFQueryRecord record, in MaterialRecord material, bool isMetal, in float2 sample) {

    float cosWiG = dot(record.geometryNormal, record.wiWorld);
    if (cosWiG <= 0) return 0.0;

    if (material.roughness < 0.01) {
        record.woShading = float3(-record.wiShading.x, -record.wiShading.y, record.wiShading.z);
        record.woWorld = material.shadingFrame.toWorld(record.woShading);
        //record.measure = EDiscrete;
        //record.eta = 1.0f;

        return glossyEval(record, material, isMetal);
    }

    if (!record.wmSet) {
        record.wm = squareToGGX(record.wiShading, sample, material.roughness);
        record.wmSet = true;
    }

    Frame microfacetFrame = Frame(record.wm);

    // reflect around microfacet
    float3 MicrofacetWi = microfacetFrame.toLocal(record.wiShading);
    float3 MicrofacetWo = float3(-MicrofacetWi.x, -MicrofacetWi.y, MicrofacetWi.z);
    record.woShading = microfacetFrame.toWorld(MicrofacetWo);
    record.woWorld = material.shadingFrame.toWorld(record.woShading);

    // handle low roughness
    //record.measure = ESolidAngle;
    //record.eta = 1.0f;

    float pdfValue = glossyPdf(record, material);
    float min = 0.001;
    float max = 1000.0;

    pdfValue = clamp(pdfValue, min, max);

    return glossyEval(record, material, isMetal) / pdfValue;
}

public float3 DiffuseEval(in BSDFQueryRecord record, in MaterialRecord material) {

    float cosWiG = dot(record.geometryNormal, record.wiWorld);
    if (cosWiG <= 0) return float3(0);
    float cosWoG = dot(record.geometryNormal, record.woWorld);
    if (cosWoG <= 0) return float3(0);

    /*
    Jacobian correction gives wrong results, but without there may be energy conservation problems!!!
    float cosWiS = max(0.0, record.wiShading.z);
    float cosWoS = max(0.0, record.woShading.z);  
    float cosineTerm = (cosWiG * cosWoS) / max(1e-6, cosWiS * cosWoG);
    */

    // return material.albedo / M_PI;
    return material.albedo / M_PI * record.woShading.z;
    //return material.albedo / M_PI * cosineTerm;
}

public float DiffusePdf(in BSDFQueryRecord record, in MaterialRecord material) {

    float cosWiG = dot(record.geometryNormal, record.wiWorld);
    if (cosWiG <= 0) return 0.0;
    float cosWoG = dot(record.geometryNormal, record.woWorld);
    if (cosWoG <= 0) return 0.0;

    return squareToCosineHemispherePdf(record.woShading);
}

public float3 sampleDiffuse(inout BSDFQueryRecord record, in MaterialRecord material, in float2 sample) {

    float cosWiG = dot(record.geometryNormal, record.wiWorld);
    if (cosWiG <= 0) return 0.0;

    record.woShading = squareToCosineHemisphere(sample);
    record.woWorld = material.shadingFrame.toWorld(record.woShading);

    record.eta = 1.0;
    record.wm = float3(0.0, 0.0, 1.0);
    // moved record.wo.z out of the shader!
    // return DiffuseEval(record, material) / DiffusePdf(record, material) * record.wo.z;
    return DiffuseEval(record, material) / DiffusePdf(record, material);
}

public float3 PrincipledEval(in BSDFQueryRecord record, in MaterialRecord material) {
    float eta = convertIORLevel(material.ior);

    if (!record.wmSet) {
        record.wm = record.woShading + record.wiShading;
        record.wm = normalize(record.wm);
    }
    
    float F = 0.0;
    float cosWiG = dot(record.geometryNormal, record.wiWorld);
    if (cosWiG > 0.0) F = fresnel(dot(record.wiShading, record.wm), eta);

    // notice: as a convention the alpha value behaves inverted compared to the metallic and translucency. A value of 1 means there is no transperency

    //float3 alphaInfluence = (1.0f - material.alpha) * transparent->eval(bRec);

    float3 metallicInfluence =  material.metalness * glossyEval(record, material, true);

    float3 specularInfluence =  (1.0f - material.metalness) * F * glossyEval(record, material, false);

    float3 diffuseInfluence =  (1.0f - material.metalness) * (1.0f - F) * DiffuseEval(record, material);

    // float3 diffuseInfluence = material.alpha * (1.0f - localTranslucency) * (1.0f - localMetallic) * (1.0f - F) * diffuse->eval(bRec);
    //Color3f translucencyInfluence = localAlpha * (localTranslucency) * (1.0f - localMetallic) * (1.0f - F) * refraction->eval(bRec);

    //return alphaInfluence + metallicInfluence + specularInfluence + diffuseInfluence + translucencyInfluence;

    return metallicInfluence + specularInfluence + diffuseInfluence;
}

public float PrincipledPdf(in BSDFQueryRecord record, in MaterialRecord material) {
    float eta = convertIORLevel(material.ior);

    if (!record.wmSet) {
        record.wm = record.woShading + record.wiShading;
        record.wm = normalize(record.wm);
    }
    
    float F = 0.0;
    float cosWiG = dot(record.geometryNormal, record.wiWorld);
    if (cosWiG > 0) F = fresnel(dot(record.wiShading, record.wm), eta);

    float metallicInfluence =  material.metalness * glossyPdf(record, material);

    float specularInfluence =  (1.0f - material.metalness) * F * glossyPdf(record, material);

    float diffuseInfluence =  (1.0f - material.metalness) * (1.0f - F) * DiffusePdf(record, material);

    return metallicInfluence + specularInfluence + diffuseInfluence;
}

public float3 samplePrincipled(inout BSDFQueryRecord record, in MaterialRecord material, inout Sampler sampler) {
    float2 sample = sampler.UniformFloat2();

    float eta = convertIORLevel(material.ior);

    if (material.roughness < 0.01) {
        record.wm = float3(0.0, 0.0, 1.0);
    } else {
        record.wm = squareToGGX(record.wiShading, sample, material.roughness);
    }
    
    record.wmSet = true;

    float F = 0.0;
    float cosWiG = dot(record.geometryNormal, record.wiWorld);
    if (cosWiG > 0) F = fresnel(dot(record.wiShading, record.wm), eta);

    float metallicInfluence = material.metalness;

    float specularInfluence = (1.0f - material.metalness) * F;

    float diffuseInfluence = (1.0f - material.metalness) * (1.0f - F);

    LightInteraction interactions[3];

    interactions[0] = LightInteraction::DIFFUSE;
    interactions[1] = LightInteraction::SPECULAR;
    interactions[2] = LightInteraction::METALLIC;

    float weights[3];

    weights[0] = diffuseInfluence;
    weights[1] = specularInfluence + weights[0];
    weights[2] = metallicInfluence + weights[1];

    float rand = sampler.UniformFloat();
    LightInteraction chosenInteraction = LightInteraction::DIFFUSE;
    for (uint i = 0; i < 3; i++) {
        if (weights[i] >= rand) {
            chosenInteraction = interactions[i];
            break;
        }
    }

    switch (chosenInteraction)
    {
    case LightInteraction::DIFFUSE:
        return sampleDiffuse(record, material, sample);
    case LightInteraction::SPECULAR:
        return sampleGlossy(record, material, false, sample);
    case LightInteraction::METALLIC:
        return sampleGlossy(record, material, true, sample);
    default:
        return float3(0.0);
    } 
}