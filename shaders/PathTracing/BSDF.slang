import Defines;
import Warp;
import Materials;
import Random;
import Surface;

public struct BSDFQueryRecord {
    public float3 geometryNormal; 

    public float3 wiWorld;
    public float3 woWorld;

    public float3 wiShading;
    public float3 woShading;

    public float3 wm;
    public float eta;
}

public BSDFQueryRecord constructBSDFRecord(in IntersectionRecord its, in MaterialRecord material) {
    BSDFQueryRecord record;
    record.wiShading = material.shadingFrame.toLocal(-its.direction);
    record.wiWorld = -its.direction;
    record.geometryNormal = its.normal;
    return record;
}

public BSDFQueryRecord constructBSDFRecord(in IntersectionRecord its, in MaterialRecord material, float3 outDirection) {
    BSDFQueryRecord record;
    record.wiShading = material.shadingFrame.toLocal(-its.direction);
    record.wiWorld = -its.direction;
    record.woShading = material.shadingFrame.toLocal(outDirection);
    record.woWorld = outDirection;
    record.geometryNormal = its.normal;
    return record;
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
    return DiffuseEval(record, material);
}

public float PrincipledPdf(in BSDFQueryRecord record, in MaterialRecord material) {
    return DiffusePdf(record, material);
}

public float3 samplePrincipled(inout BSDFQueryRecord record, in MaterialRecord material, inout Sampler sampler) {
    float2 sample = sampler.UniformFloat2();
    return sampleDiffuse(record, material, sample);
}