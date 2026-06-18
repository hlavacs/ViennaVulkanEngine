// Shadow miss shader for VERendererRaytracing.
// Reached only when a shadow ray hits nothing, i.e. the point is lit.
#include "common.slang"

[shader("miss")]
void rmissShadowMain(inout ShadowPayload payload)
{
    payload.inShadow = false;
}
