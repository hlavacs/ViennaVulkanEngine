import raytracing;
import Surface;
import Materials;
import Frame;
import Random;
import Warp;
import IR_Defines;
import IR_Bindings;
import BSDF;
import IR_Common;

float3 getUnoccludedContributionFromVPL(in IntersectionRecord itsCamera, in MaterialRecord materialCamera, in VPLReconnectionInfo vpl) {
    float3 itsCamToVPL = vpl.position.xyz - itsCamera.position;

    float distance = length(itsCamToVPL);
    float3 wo = normalize(itsCamToVPL);

    float cosCam = dot(itsCamera.normal, wo);
    float cosVPL = dot(vpl.normal.xyz, -wo);

    BSDFQueryRecord bsdfRecCamera = constructBSDFRecord(itsCamera, materialCamera, wo, true);

    float3 bsdf = PrincipledEvalNoCosine(bsdfRecCamera, materialCamera);

    //this might be incorrect. I think I might need to do distance swared devided by cosine and not the other way around to get the area pdf 
    // but that is very stange
    //solution: I need to devide the radiance by the pdf aka I thake radiance / areaPDF * distance^2/cos(tetha)
    //Is that the same as (radiance / areaPDF) * cos(tetha) / distance^2

    // solidPDF = areaPDF * distance^2 / cos(tetha)
    // I know areaPDF I need solid angle pdf
    // what I need in the end is Radiance / solidPDF
    // That is:  radiance / (areaPDF * distance^2/cos(tetha))
    // but I right now only have (radiance/areaPDF)
    // I can do (radiance/areaPDF) / distance^2/cos(tetha) which is the same as radiance / areaPDF * distance^2/cos(tetha)
    // I can then flip: (radiance / areaPDF) * (cos(tetha) / distance^2) getting the correct result :D

    //cosCam is the solid angle term that I would usually put into the shader

    distance = max(distance, 0.01);

    float geometryTerm = (cosCam * cosVPL) / (distance * distance);

    return vpl.weightedRadiance.xyz * bsdf * geometryTerm;
}

float3 getOccludedContributionFromVPL(in IntersectionRecord itsCamera, in MaterialRecord materialCamera, in VPLReconnectionInfo vpl) {
    if (isPointOccluded(itsCamera.position, vpl.position.xyz, itsCamera.normal)) {
        return float3(0.0);
    }

    return getUnoccludedContributionFromVPL(itsCamera, materialCamera, vpl);
}