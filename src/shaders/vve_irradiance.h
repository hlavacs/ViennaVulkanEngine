/** @file @brief Public irradiance atlas layout and units, shared by C++, engine and native Slang samplers. */
#ifndef VVE_IRRADIANCE_CONSTANTS
#define VVE_IRRADIANCE_CONSTANTS
#define VVE_PROBE_SIDE 4 ///< World-anchored irradiance probes per grid axis.
#define VVE_PROBE_COUNT (VVE_PROBE_SIDE * VVE_PROBE_SIDE * VVE_PROBE_SIDE) ///< SH rows, x varying fastest.
#define VVE_PROBE_WRAP(cell) ((((cell) % VVE_PROBE_SIDE) + VVE_PROBE_SIDE) % VVE_PROBE_SIDE) ///< Positive modulo, including negative world cells.
#define VVE_PROBE_ROW(x,y,z) (VVE_PROBE_WRAP(x) + VVE_PROBE_SIDE * (VVE_PROBE_WRAP(y) + VVE_PROBE_SIDE * VVE_PROBE_WRAP(z))) ///< Toroidal world-cell storage, identical in C++ and Slang.
#define VVE_PROBE_SH_COEFFICIENTS 4 ///< L1 RGB SH: constant and three first-order directional coefficients.
#define VVE_PROBE_FACE_SIZE 16 ///< Texels per cubemap face in the bounded radiance atlas.
#define VVE_PROBE_MAX_BUDGET 2 ///< Complete cubemaps captured per window frame at medium quality.
#define VVE_PROBE_SPACING 2.0f ///< Metres between adjacent probe centres.
#define VVE_PROBE_CASCADES 2 ///< Independent near/coarse toroidal volumes per window.
#define VVE_PROBE_SCALE(cascade) ((cascade) == 0 ? 1.0f : 4.0f) ///< Coarse distances scale uniformly, including capture clips and moments.
#define VVE_PROBE_BLEND_CELLS 1.0f ///< Fade over one cell inside each volume boundary (2 m near, 8 m coarse).
#define VVE_PROBE_SHADOW_RESOLUTION 512 ///< One cached D32 sun map per window, independent of camera cascades.
#define VVE_PROBE_SHADOW_PADDING_CELLS 1.0f ///< Pad the union by one coarse cell for nearby capture surfaces.
#define VVE_PROBE_CAPTURE_NEAR 0.05f ///< Near clipping distance in metres for every capture face.
#define VVE_PROBE_CAPTURE_FAR 64.0f ///< Far clipping distance in metres for every capture face.
#define VVE_PROBE_VISIBILITY_SIDE 8 ///< Octahedral distance-moment texels per axis, flattened after each probe's SH.
#define VVE_PROBE_ATLAS_COLUMNS (VVE_PROBE_SH_COEFFICIENTS + VVE_PROBE_VISIBILITY_SIDE * VVE_PROBE_VISIBILITY_SIDE) ///< RGBA32F texels per probe.
#define VVE_PROBE_VISIBILITY_EXPONENT 64.0f ///< Directional cosine-power kernel for radial distance moments.
#define VVE_PROBE_NORMAL_BIAS 0.05f ///< Metres along the shading normal for visibility queries only.
#define VVE_PROBE_VIEW_BIAS 0.02f ///< Metres toward the main eye for visibility queries only.
#define VVE_PROBE_INACTIVE_BACKFACE 0.6f ///< Inactivate captures with more than 60% back-facing solid angle.
#define VVE_PROBE_INACTIVE_NEAR 0.25f ///< Inactivate captures with more than 25% hits within twice the near clip.
#endif
