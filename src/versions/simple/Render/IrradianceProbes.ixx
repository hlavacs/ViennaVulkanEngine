module;
#include <vulkan/vulkan_core.h>
#include "../shaders/simple_shared.h"
export module VVEngine.Simple.IrradianceProbes;
import std;
import VVEngine.Simple.Types;
import VVEngine.Simple.Vulkan;

/** @file @brief Window-local bounded raster capture storage. The renderer records captures and L1 SH integration. */
export namespace vve::simple {
/// @brief One snapped local volume; images are serialized on the graphics queue, CPU uploads are fenced per slot.
struct IrradianceProbes {
    static constexpr std::uint32_t side{VVE_PROBE_SIDE}; ///< Probes per axis; same header as Slang, x changes fastest.
    static constexpr std::uint32_t count{side * side * side}; ///< Fixed volume capacity.
    static constexpr std::uint32_t shCoefficients{VVE_PROBE_SH_COEFFICIENTS}; ///< L1 coefficient columns, independent of grid side.
    static constexpr std::uint32_t visibilitySide{VVE_PROBE_VISIBILITY_SIDE}; ///< Directional moment map width and height.
    static constexpr std::uint32_t atlasColumns{VVE_PROBE_ATLAS_COLUMNS}; ///< Packed SH and flattened visibility columns per probe.
    static constexpr std::uint32_t faceSize{VVE_PROBE_FACE_SIZE}; ///< Cubemap face width and height.
    static constexpr std::uint32_t maxBudget{VVE_PROBE_MAX_BUDGET}; ///< Maximum complete captures per window frame.
    static constexpr std::uint32_t faces{VVE_POINT_SHADOW_FACES}; ///< Cubemap views per complete capture.
    static constexpr std::uint32_t views{faces * maxBudget}; ///< Private uniform count per fenced frame slot.
    static constexpr Scalar spacing{VVE_PROBE_SPACING}; ///< World metres between adjacent probes.
    static constexpr Scalar captureNear{VVE_PROBE_CAPTURE_NEAR}; ///< Near capture clipping distance in metres.
    static constexpr Scalar captureFar{VVE_PROBE_CAPTURE_FAR}; ///< Far capture clipping distance in metres.
    // The atlas allocation, grid sampling and quadrature must retain the shared Slang header's dimensions.
    static_assert(side == VVE_PROBE_SIDE && count == VVE_PROBE_COUNT && faceSize == VVE_PROBE_FACE_SIZE &&
        maxBudget == VVE_PROBE_MAX_BUDGET && spacing == VVE_PROBE_SPACING && shCoefficients == VVE_PROBE_SH_COEFFICIENTS &&
        visibilitySide == VVE_PROBE_VISIBILITY_SIDE && atlasColumns == shCoefficients + visibilitySide * visibilitySide);
    static_assert(side > 1 && faceSize > 0 && maxBudget > 0 && maxBudget <= count && captureNear > 0 && captureFar > captureNear);
    VulkanImage radiance{}; ///< Linear HDR RGB and signed radial distance in metres (negative for backfaces).
    VulkanImage depth{}; ///< Raster capture depth, not probe visibility storage.
    VulkanImage coefficients{}; ///< SH with validity alpha, followed by radial mean/metres, second moment/metres squared and hit fractions.
    VulkanUniformBuffers uniforms{}; ///< Six distinct view uniforms per updated probe and fenced slot.
    VulkanDescriptorPool pool{}; ///< Capture sets preserved on resize; quality replaces their owner through submission fences.
    VulkanDescriptorSets sets{}; ///< Copies of current slot's scene bindings with private capture uniforms.
    Scalar scale{1}; ///< Shared cascade distance multiplier; near=1, coarse=VVE_PROBE_SCALE(1).
    Vec3 origin{}; ///< Snapped world coordinate of grid cell (0,0,0).
    std::array<std::uint64_t, count> captured{}; ///< Capture age by storage row; zero means never captured at this location.
    std::array<std::uint64_t, count> generation{}; ///< Lighting/geometry generation of each completed capture, including inactive probes.
    std::array<std::uint32_t, maxBudget> selected{}; ///< Arbitrary storage rows selected for this frame's bounded captures.
    std::bitset<count> clearRows{}; ///< Newly exposed rows to clear, including moments, before any lighting read.
    std::uint64_t clock{}; ///< Near owner's monotonic age is shared by all cascades for globally oldest replacement.
    std::uint32_t updated{}; ///< Number of probes captured by the current window frame.
    std::uint32_t valid{}; ///< Captured rows since reset; active/inactive counts require opt-in GPU validity readback.
    std::uint32_t stale{}; ///< Captured rows older than the latest lighting/geometry invalidation, including inactive rows.
    bool initialized{}; ///< Whether shader-readable atlas layouts have been established.
    bool reset{true}; ///< Whether validity must be cleared before lighting.
    std::uint64_t gpuBytes{}; ///< Actual VMA image/buffer allocation bytes, excluding driver-owned descriptors/pipelines.
    void cleanup();
};
/// @brief Releases sets before views/buffers; safe only after the owning target's completion boundary.
inline void IrradianceProbes::cleanup() {
    sets.cleanup(); pool.cleanup(); uniforms.cleanup();
    radiance.cleanup(); depth.cleanup(); coefficients.cleanup();
    captured.fill(0); generation.fill(0); clearRows.reset(); clock = 0;
    updated = valid = stale = 0; initialized = false; reset = true; gpuBytes = 0;
}
}
