// Basic miss shader for VERendererRaytracing.
// Returns a constant background color when a ray hits no geometry.
#include "common.slang"

[shader("miss")]
void rmissMain(inout HitPayload payload)
{
    // Solid background color for rays that miss all geometry.
    payload.color = float3(0.1, 0.2, 0.4);
}
