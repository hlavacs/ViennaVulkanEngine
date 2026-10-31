module;
#include <vulkan/vulkan_core.h>
#include <vk_mem_alloc.h>
module VVEngine.Simple.Renderer;
import std;

/** @file @brief Transactional GI replacement, bounded ownership and opt-in allocation diagnostics. */
namespace vve::simple {
/// @brief Measures GI-owned VMA storage; G-buffer/HDR/depth are shared deferred infrastructure and reported separately.
std::uint64_t ForwardRenderer::giGpuBytes(const WindowTarget &target) {
    return target.probes.gpuBytes + target.coarseProbes.gpuBytes + target.probeShadowBytes + target.aoGpuBytes;
}
/// @brief Includes old probe generations still owned by submissions, so a rapid quality toggle cannot hide memory growth.
std::uint64_t ForwardRenderer::giGpuBytes() const {
    std::uint64_t bytes{};
    for (const auto &target : targets) bytes += giGpuBytes(target);
    for (const auto &[serial, resource] : retiredResources_) {
        if (const auto *target = std::get_if<std::unique_ptr<WindowTarget>>(&resource)) bytes += giGpuBytes(**target);
    }
    return bytes;
}
/// @brief Queries VMA only when requested by diagnostics; zero after failed initialization or teardown.
std::uint64_t ForwardRenderer::gpuAllocationCount() const {
    if (allocator.allocator == VK_NULL_HANDLE) return 0;
    VmaTotalStatistics statistics{};
    vmaCalculateStatistics(allocator, &statistics);
    return statistics.total.statistics.allocationCount;
}
/// @brief Fails once after N successful checkpoints, allowing tests to exercise real partial owners and clean retries.
bool ForwardRenderer::failGiAllocation() {
    if (!debugGiFailAfter) return false;
    if (*debugGiFailAfter != 0) { --*debugGiFailAfter; return false; }
    debugGiFailAfter.reset();
    return true;
}
/// @brief Replaces all windows transactionally; the existing graphics fences retire the previous images, sets and uniforms.
auto ForwardRenderer::setGiSettings(GiSettings settings) -> std::expected<void, Error> {
    if (!std::isfinite(settings.indirect_strength) || settings.indirect_strength < zero() ||
        (settings.quality != GiQuality::low && settings.quality != GiQuality::medium)) return std::unexpected(Error::invalid_argument);
    if (settings.quality == giSettings.quality || !initialized() || backend_ != RendererBackend::hybrid_gi) {
        giSettings = settings; return {};
    }
    // Reserve the per-window ceiling before allocating; retired generations count until their submission fences complete.
    if (giGpuBytes() + targets.size() * probeByteBudget > giTotalByteBudget) return std::unexpected(Error::capacity_exceeded);
    std::vector<std::pair<WindowTarget *, std::unique_ptr<WindowTarget>>> replacements;
    for (auto &target : targets) {
        auto pending = std::make_unique<WindowTarget>();
        if (createProbeResources(*pending) != VK_SUCCESS) return std::unexpected(Error::platform_error);
        replacements.emplace_back(&target, std::move(pending));
    }
    // A partial target bundles only probe owners and the sampled set; screen attachments and AO stay in their original window.
    const auto exchange = [](WindowTarget &target, WindowTarget &pending) {
        std::swap(target.probes, pending.probes); std::swap(target.coarseProbes, pending.coarseProbes);
        std::swap(target.probeShadow, pending.probeShadow); std::swap(target.probeShadowBytes, pending.probeShadowBytes);
        std::swap(target.deferredPool, pending.deferredPool); std::swap(target.deferredSet, pending.deferredSet);
    };
    std::size_t exchanged{};
    for (auto &[target, pending] : replacements) {
        exchange(*target, *pending); ++exchanged;
        if (createDeferredDescriptors(*target) == VK_SUCCESS) continue;
        // Restore every previous descriptor/image pair; pending destructors release only the never-submitted replacements.
        for (auto &replacement : replacements | std::views::take(exchanged)) exchange(*replacement.first, *replacement.second);
        return std::unexpected(Error::platform_error);
    }
    for (auto &[target, previous] : replacements) {
        target->probeShadowInitialized = false; target->probeShadowDirty = true;
        retire(std::move(previous));
    }
    giSettings = settings;
    return {};
}
} // namespace vve::simple
