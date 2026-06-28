import Defines;
import Materials;
import Lightsource;
import Random;
import Frame;
import BSDF;
import ColorSpace;
import Surface;
import raytracing;
import Reservoir;
import RestirDI;

void clampReadID(inout int2 readID, in int2 launchSize) {
    if (readID.x < 0) {
        readID.x = 0;
    }
    if (readID.y < 0) {
        readID.y = 0;
    }
    if (readID.x > launchSize.x - 1) {
        readID.x = launchSize.x - 1;
    }
    if (readID.y > launchSize.y - 1) {
        readID.y = launchSize.y - 1;
    }
}

void storeEmptyReservoir(in int2 launchID, in int2 launchSize, in RWStructuredBuffer<Reservoir> outReservoir) {
    Reservoir reservoir = createEmptyReservoir();
    storeReservoir(reservoir, launchID, launchSize, outReservoir);
}

float3 restirSpatialDI(in int2 launchID, in int2 launchSize, inout Sampler sampler, in MaterialRecord material, in IntersectionRecord its,
                       in RWTexture2D<float4> inNormal, in RWTexture2D<float4> inPosition, 
                       in RWStructuredBuffer<Reservoir> inReservoir, in RWStructuredBuffer<Reservoir> outReservoir) {
        float lightPdf = 1.0 / float(lightCount);
        float p_hat = 0;
        const uint MAX_SPATIAL_INFLUENCE = 10;
        const uint SPATIAL_SAMPLES = spatialSamplesRestirDI;
        const uint SAMPLE_RADIUS = spatialRadiusRestirDI;

        Reservoir reservoir = readReservoir(launchID, launchSize, inReservoir);
        
        //normal may be inverted in its so dont use that here 
        float3 normal = inNormal[launchID].xyz;
        float3 position = inPosition[launchID].xyz;

        for (uint i = 0; i < SPATIAL_SAMPLES; i++) {
            int x = sampler.UniformUIntFast(0, 2 * SAMPLE_RADIUS) - SAMPLE_RADIUS;
            int y = sampler.UniformUIntFast(0, 2 * SAMPLE_RADIUS) - SAMPLE_RADIUS;

            int2 offset = int2(x, y);

            int2 readID = launchID + offset;
            clampReadID(readID, launchSize);

            float3 normalSpatial = inNormal[readID].xyz;
            float3 positionSpatial = inPosition[readID].xyz;

            if (dot(normal, normalSpatial) >= 0.9 && length(positionSpatial - position) < 0.25) {
                Reservoir reservoirSpatial = readReservoir(readID, launchSize, inReservoir);

                // restrict influence from past samples.
                // this may not be optimal, the real time raytracing engine handles this diffrently
                reservoirSpatial.M = min(MAX_SPATIAL_INFLUENCE * reservoir.M, reservoirSpatial.M);

                if(IsReservoirValid(reservoirSpatial)){
                    reservoir = CombineReservoirs(reservoir, reservoirSpatial, its, material, sampler);
                }
                
            }     
        }

        float3 output = float3(0.0);

        if (IsReservoirValid(reservoir))
        {
            float3 radiance = revaluateOccludedLightRadiance(reservoir, its, material);
            //float3 radiance = resampleOccludedLightRadiance(reservoir, its, material, sampler);
            
            output = radiance * reservoir.lightWeight;
            float p_hat = length(radiance);
            //maybe one shouldn't use this but I am unsure???

            //this does not work because one has to sample the same point on the light source every time otherwhise restir does not work
            /*
            if(p_hat <= 0.0 || reservoir.M == 0) {
                reservoir = Reservoir(0, 0.0, 0.0, 0);
            }
                */
                
            //reservoir.lightWeight = p_hat > 0.0 ? (1.0 / max(p_hat, 0.001)) * reservoir.W_sum / reservoir.M : 0.0;
        } else {
            reservoir = createEmptyReservoir();
        }

        storeReservoir(reservoir, launchID, launchSize, outReservoir);

        return output;
}
