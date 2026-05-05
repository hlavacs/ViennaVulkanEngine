// Basic closest hit shader for VERendererRaytracing.
// Returns a constant color for any geometry that is hit.
#include "common.slang"

[shader("closesthit")]
void rchitMain(inout HitPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    // Solid color for any surface hit.
    payload.color = float3(1.0, 0.5, 0.0);
}
