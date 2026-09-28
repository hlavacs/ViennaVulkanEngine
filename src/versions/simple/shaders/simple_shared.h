/// @file
/// @brief Constants shared by the simple renderer's C++ code and Slang shaders.
#define VVE_MAX_SCENE_TEXTURES 64
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
#define VVE_MATERIAL_BINDING 5
#define VVE_SHADOW_NEAR_PLANE 0.1f              ///< Near plane used by all shadow projections.
#define VVE_SHADOW_OCCLUDED_FACTOR 0.35f       ///< Partial-light floor for occluded shadow samples.
#define VVE_DIRECTIONAL_SHADOW_BIAS 0.00005f   ///< Receiver bias for the nearest directional cascade.
#define VVE_PERSPECTIVE_SHADOW_WORLD_BIAS 0.02f ///< Two-centimetre receiver offset in world units.
#define VVE_PERSPECTIVE_SHADOW_EPSILON 0.001f   ///< Positive denominator floor near the light origin.
#define VVE_PERSPECTIVE_SHADOW_MIN_BIAS 0.000002f ///< Minimum offset in nonlinear depth units.
