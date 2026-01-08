import Defines;

public float2 squareToUniformDisk(in float2 sample) {
	float r = sqrt(sample.x);
	float phi = 2.0f * M_PI * sample.y;

    float sinPhi = sin(phi);
    float cosPhi = cos(phi);

	float x = r * cosPhi;
	float y = r * sinPhi;

    return float2(x,y);
}



public float squareToUniformDiskPdf(in float2 p) {
    float lenght = sqrt(p.x * p.x + p.y * p.y);

    if (lenght > 1.0) {
        return 0;
    } else {
        return 1 / (M_PI);
    }
}

public float3 squareToUniformSphere(in float2 sample)
{
    // z is uniform in [-1,1]
    float z = 1.0f - 2.0f * sample.y;

    // radius in xy plane
    float r = sqrt(max(0.0f, 1.0f - z * z));

    float phi = 2.0f * M_PI * sample.x;

    float sinPhi = sin(phi);
    float cosPhi = cos(phi);

    return float3(r * cosPhi, r * sinPhi, z);
}

// This potentially generates a cosine sample
public float3 squareToUniformHemisphere(in float2 sample) {
    // sample.y is our cos(theta). For a uniform distribution,
    // we sample the height (z) linearly between 0 and 1.
    float z = sample.y;

    // Calculate the radius in the XY plane using the identity:
    // sin^2(theta) + cos^2(theta) = 1  =>  r = sin(theta)
    float r = sqrt(max(0.0, 1.0 - z * z));

    // Sample the azimuth (phi) uniformly around the circle
    float phi = 2.0 * M_PI * sample.x;

    float sinPhi = sin(phi);
    float cosPhi = cos(phi);

    return float3(r * cosPhi, r * sinPhi, z);
}

public float squareToUniformHemispherePdf(in float3 v) {
    return 1 / (2.0f * M_PI);
}

public float3 squareToCosineHemisphere(in float2 sample) {
    float2 uniformDisk = squareToUniformDisk(sample);
    float z = sqrt(1 - uniformDisk.x * uniformDisk.x - uniformDisk.y * uniformDisk.y);
    return float3(uniformDisk.x, uniformDisk.y, z);
}

public float squareToCosineHemispherePdf(in float3 v) {
    if (v.z > 0.0) {
        return v.z / (M_PI);
    } else {
        return 0.0;
    }
}

public float3 squareToGGX(in float3 wi, in float2 sample, float roughness) {
    float3 macronormal = float3(0.0, 0.0, 1.0);

    float3 wh = float3(roughness * wi.x, roughness * wi.y, wi.z);
    normalize(wh);
    if (wh.z < 0) {
        wh = -wh;
    }

    float3 BasisVector1 = (wh.z < 0.99999f) ? (normalize(cross(macronormal, wh))) : float3(1, 0, 0);
    float3 BasisVector2 = cross(wh, BasisVector1);

    float2 p = squareToUniformDisk(sample);

    float h = sqrt(1 - p.x * p.x);

    float scale = 0.5 * (1 + wh.z);
    float offset = (h / 2.0) * (1 - wh.z);

    p.y = (p.y * scale) + offset;

    float lenghtsquared = p.x * p.x + p.y * p.y;
    float pz = sqrt(max(0.0, 1.0 - lenghtsquared));
    float3 nh = p.x * BasisVector1 + p.y * BasisVector2 + pz * wh;
    float3 SampledNormal = float3(roughness * nh.x, roughness * nh.y, max(1e-6f, nh.z));
    return normalize(SampledNormal);
}

public float G1(in float3 wi, float roughness) {
    return (2.0 * wi.z) / (wi.z + sqrt(roughness * roughness + (1.0 - roughness * roughness) * wi.z * wi.z));
}
public float G(in float3 wi, in float3 wo, float roughness) {
    return G1(wi, roughness) * G1(wo, roughness);
}

public float D(in float3 micronormal, float roughness) {

    // D(wm) return a constant, this constant gives the relative differential area of microfacets with the surface normal (needed for normalization)
    // r^2/(pi * (cos(theta)^2 * (r^2 - 1) + 1)^2)

    float alpha = roughness * roughness;

    float m = micronormal.z * micronormal.z * (alpha - 1) + 1;

    return alpha / (M_PI * m * m);

    //return std::pow(roughness, 2.0) / (M_PI * std::pow(std::pow(micronormal.z(), 2.0) * (std::pow(roughness, 2.0) - 1) + 1, 2.0));
}

public float squareToGGXPDF(in float3 wi, in float3 micronormal, float roughness) {
    // This is D(wi,wm) handels part of the masking and shadowing specifically this term does:
    // std::abs(wi.dot(micronormal))

    // need to limit to -90° to 90°
    if (wi.z > 0.0 && micronormal.z < 0.0) {
        return 0.0;
    }
    if (wi.z < 0.0 && micronormal.z > 0.0) {
        return 0.0;
    }
    // eliminate backfacing
    if (dot(wi, micronormal) <= 0.0) {
        return 0.0;
    }
    return G1(wi, roughness) / abs(wi.z) * D(micronormal, roughness) * abs(dot(wi, micronormal));
}