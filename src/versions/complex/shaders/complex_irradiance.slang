/** @file @brief Connect the shared public irradiance sampler to the renderer's private frame ABI. */
#define VVE_IRRADIANCE_SET 1
#define VVE_IRRADIANCE_NEAR_BINDING 4
#define VVE_IRRADIANCE_COARSE_BINDING 8
#define VVE_IRRADIANCE_ORIGIN frame.probeOrigin
#define VVE_IRRADIANCE_EYE worldEye()
#define VVE_IRRADIANCE_VISIBILITY frame.probeControl.z
#define VVE_IRRADIANCE_CASCADES frame.viewport.w
#include "../../../shaders/vve_irradiance.slang"
