/// @file
/// @brief Constants shared by the simple renderer's C++ code and Slang shaders.
#define VVE_TEXTURE_CAPACITY_CONSTANT_ID 0 ///< Pipeline specialization selects the material texture-array size.
#define VVE_MAX_SHADOWED_SPOT_LIGHTS 10
#define VVE_MAX_SHADOWED_POINT_LIGHTS 10
#define VVE_MAX_DIRECTIONAL_LIGHTS 10
#define VVE_NUM_SHADOW_CASCADES 4
#define VVE_POINT_SHADOW_FACES 6 ///< Cubemap directions rendered per point light.
#define VVE_SHADOW_MATRIX_POINT_BASE VVE_MAX_SHADOWED_SPOT_LIGHTS ///< Point matrices follow the spot matrices.
#define VVE_SHADOW_MATRIX_DIR_BASE (VVE_SHADOW_MATRIX_POINT_BASE + VVE_MAX_SHADOWED_POINT_LIGHTS * VVE_POINT_SHADOW_FACES) ///< Directional matrices follow point faces.
#define VVE_SHADOW_MATRIX_COUNT (VVE_SHADOW_MATRIX_DIR_BASE + VVE_MAX_DIRECTIONAL_LIGHTS * VVE_NUM_SHADOW_CASCADES) ///< Total shared shadow matrices.
#define VVE_SHADOW_MAP_RESOLUTION 1024
#define VVE_NO_TEXTURE 0xFFFFFFFFu
#define VVE_ALPHA_CUTOFF 0.5f ///< Default material mask threshold; equality is opaque in every material pass.
#define VVE_OBJECT_TERRAIN_FLAG 16u ///< Tessellated grids keep their existing two-sided shading in hybrid material passes.
#define VVE_MATERIAL_LEGACY_SIDED 0u ///< Engine-authored materials keep their original main/capture policies.
#define VVE_MATERIAL_SINGLE_SIDED 1u ///< Imported single-sided material emits no back-face capture radiance.
#define VVE_MATERIAL_DOUBLE_SIDED 2u ///< Imported two-sided material shades both sides in main and capture views.
#define VVE_MATERIAL_BINDING 5
#define VVE_TERRAIN_HEIGHT_BINDING 6 ///< StructuredBuffer<float> of all addTerrain height grids, read by the tessellation stages.
#define VVE_TERRAIN_PATCH_CELLS 32 ///< Grid cells along one terrain patch edge; also its highest useful tessellation level.
#define VVE_SHADOW_NEAR_PLANE 0.1f              ///< Near plane used by all shadow projections.
#define VVE_SHADOW_OCCLUDED_FACTOR 0.35f       ///< Partial-light floor for occluded shadow samples.
#define VVE_DIRECTIONAL_SHADOW_BIAS 0.00005f   ///< Receiver bias for the nearest directional cascade.
#define VVE_PERSPECTIVE_SHADOW_WORLD_BIAS 0.02f ///< Two-centimetre receiver offset in world units.
#define VVE_PERSPECTIVE_SHADOW_EPSILON 0.001f   ///< Positive denominator floor near the light origin.
#define VVE_PERSPECTIVE_SHADOW_MIN_BIAS 0.000002f ///< Minimum offset in nonlinear depth units.
#include "../../../shaders/vve_irradiance.h"
#define VVE_AO_RADIUS 0.5f ///< Contact visibility radius in world metres, independent of resolution and exposure.
#define VVE_AO_DIRECTIONS 8 ///< Rotated screen-space horizon directions per pixel.
#define VVE_AO_STEPS 4 ///< Radial depth taps per direction (32 total).
#define VVE_AO_BIAS 0.02f ///< Metres above the tangent plane to reject coplanar self-occlusion.
#define VVE_AO_NORMAL_COS 0.9f ///< Minimum normal similarity for spatial filtering and coverage-sample reuse.
#define VVE_AO_DEPTH_RELATIVE 0.01f ///< Maximum relative linear view-depth difference when reusing sample-zero AO.
#define VVE_AO_FILTER_DEPTH 0.05f ///< Bilateral linear-depth scale in metres; gaps above this are rejected.
