import Defines;
import Warp;
import Materials;
import Random;

public struct BSDFQueryRecord {
    public float3 wi;
    public float3 wo;
    public float3 wm;
    public float eta;
}

public float3 DiffuseEval(in BSDFQueryRecord record, in MaterialRecord material) {
   
    if (record.wi.z < 0.0) {
        return float3(0.0);
    }
    if (record.wo.z < 0.0) {
        return float3(0.0);
    }

    // return material.albedo / M_PI;
    return float3(0.5, 0.5, 0.5) / M_PI * record.wo.z;
}

public float DiffusePdf(in BSDFQueryRecord record, in MaterialRecord material) {
    
    if (record.wi.z < 0.0) {
        return 0.0;
    }
    if (record.wo.z < 0.0) {
        return 0.0;
    }
        
    return squareToCosineHemispherePdf(record.wo);
}

public float3 sampleDiffuse(inout BSDFQueryRecord record, in MaterialRecord material, in float2 sample) {
    
    if (record.wi.z < 0.0) {
        return float3(0.0, 0.0, 0.0);
    }
        

    record.wo = squareToCosineHemisphere(sample);
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