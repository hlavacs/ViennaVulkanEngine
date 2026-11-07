/** @file vve_surface_flags.slang @brief Public native-surface flags packed as materialAO + 2*flags.
 * These shading flags are independent of implementation geometry/object flags.
 */
#ifndef VVE_SURFACE_FLAGS_INCLUDED
#define VVE_SURFACE_FLAGS_INCLUDED
static const uint VVE_SURFACE_UNLIT=1u;
static const uint VVE_SURFACE_DIFFUSE_ONLY=2u; ///< Lambert diffuse and diffuse GI/sky; no direct or environment specular.
static const uint VVE_SURFACE_ADDITIVE_AMBIENT=4u; ///< Authored ambient is an additional fill light, not the diffuse environment fallback replaced by GI.
// Bit 8 is reserved by built-in deferred water shading.
static const uint VVE_SURFACE_MULTIPLY_CONTACT_AO=16u; ///< Material visibility and screen contact visibility describe separate scales; combine multiplicatively.
#endif