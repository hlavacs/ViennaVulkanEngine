/** @file @brief Opt-in GPU pass receipts; query reads and reuse follow existing window-slot fences. */
module;
#include <vulkan/vulkan_core.h>
#include <vk_mem_alloc.h>
module VVEngine.Simple.Renderer;
import std;

namespace vve::simple {
/// @brief Discards old receipts on a diagnostic toggle; pending GPU owners remain alive until ordinary cleanup.
void ForwardRenderer::setGpuTimingReadback(bool enabled) {
    gpuTimingEnabled = enabled;
    for (auto &target : targets) {
        target.gpuMs.fill(std::nullopt); target.aoGpuMs.reset(); target.timingSerial = 0;
        for (auto &written : target.timingWritten) written.reset();
    }
}
/// @brief Lazily allocates queries only for diagnostic runs; unsupported timestamps leave receipts unavailable.
VkResult ForwardRenderer::prepareGpuTiming(WindowTarget &target) {
    if (!gpuTimingEnabled || target.gpuQueries) return VK_SUCCESS;
    std::uint32_t count{};
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice.physicalDevice, &count, nullptr);
    std::vector<VkQueueFamilyProperties> queues(count);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice.physicalDevice, &count, queues.data());
    gpuTimestampBits = queues[*physicalDevice.graphicsQueueFamily].timestampValidBits;
    VkPhysicalDeviceProperties properties{}; vkGetPhysicalDeviceProperties(physicalDevice.physicalDevice, &properties);
    gpuTimestampPeriod = properties.limits.timestampPeriod;
    if (!gpuTimestampBits || !(gpuTimestampPeriod > 0)) return VK_SUCCESS;
    const VkQueryPoolCreateInfo info{.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        .queryType = VK_QUERY_TYPE_TIMESTAMP, .queryCount = WindowTarget::timingQueries * framesInFlight};
    VkQueryPool pool{};
    const auto result = vkCreateQueryPool(device.device, &info, nullptr, &pool);
    return target.gpuQueries.assign(device.device.handle, result, pool);
}
/// @brief Marks completed graphics work at both boundaries; diagnostic serialization includes pass barriers/stores.
void ForwardRenderer::writeGpuTimestamp(WindowTarget &target, VkCommandBuffer command, unsigned query) {
    if (!gpuTimingEnabled || !target.gpuQueries) return;
    vkCmdWriteTimestamp(command, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, target.gpuQueries,
        target.currentFrame * WindowTarget::timingQueries + query);
    // The final frame marker is published only after successful queue submission, excluding abandoned recordings.
    if (query + 1 < WindowTarget::timingQueries) target.timingWritten[target.currentFrame].set(query);
}
/// @brief Called only after this slot's existing fence signalled; availability is checked without WAIT or partial results.
void ForwardRenderer::collectGpuTiming(WindowTarget &target, std::uint32_t frame) {
    auto &written = target.timingWritten[frame];
    if (!gpuTimingEnabled || !target.gpuQueries || !written.test(WindowTarget::timingQueries - 1)) return;
    target.gpuMs.fill(std::nullopt); target.timingSerial = target.submitSerials[frame];
    const auto mask = std::numeric_limits<std::uint64_t>::max() >> (64 - gpuTimestampBits);
    // Two cascade pairs accumulate into one capture/integration measurement; absent passes have measured zero work.
    constexpr std::array first{0U, 4U, 8U, 10U, 12U}, pairs{2U, 2U, 1U, 1U, 1U};
    // Establish frame availability first, so omitted passes never masquerade as ready before any frame result exists.
    for (auto metric : std::array{4U, 0U, 1U, 2U, 3U}) {
        double elapsed{}; bool ready = true;
        for (auto pair : std::views::iota(0U, pairs[metric])) {
            const auto query = first[metric] + pair * 2;
            if (!written.test(query) || !written.test(query + 1)) continue;
            std::array<std::array<std::uint64_t,2>,2> values{}; // Timestamp and availability, per query.
            const auto result = vkGetQueryPoolResults(device.device, target.gpuQueries,
                frame * WindowTarget::timingQueries + query, 2, sizeof(values), values.data(), sizeof(values[0]),
                VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
            if (result != VK_SUCCESS || !values[0][1] || !values[1][1]) { ready = false; break; }
            elapsed += double((values[1][0] - values[0][0]) & mask) * gpuTimestampPeriod / 1e6;
        }
        if (ready) target.gpuMs[metric] = elapsed;
        else if (metric == 4) break;
    }
    target.aoGpuMs = target.gpuMs[2]; written.reset();
}
/// @brief Copies small receipts on explicit request; no wait, query read or pixel transfer occurs here.
auto ForwardRenderer::renderDiagnostics(WindowHandle window) const -> std::expected<Vector<std::pair<std::string,std::string>>,Error> {
    const auto target = std::ranges::find(targets, window, &WindowTarget::handle);
    if (target == targets.end()) return std::unexpected(Error::invalid_handle);
    Vector<std::pair<std::string,std::string>> receipt;
    constexpr std::array names{"gi.capture_ms", "gi.integrate_ms", "ao.gpu_ms", "gi.lighting_ms", "gpu.frame_ms"};
    for (auto i : std::views::iota(0U, unsigned(names.size())))
        receipt.emplace_back(names[i], target->gpuMs[i] ? std::format("{:.6f}", *target->gpuMs[i]) : "unavailable");
    receipt.emplace_back("gpu.timing_serial", std::to_string(target->timingSerial));
    receipt.emplace_back("gpu.timestamp_valid_bits", std::to_string(gpuTimestampBits));
    receipt.emplace_back("gpu.timestamp_period_ns", std::to_string(gpuTimestampPeriod));
    VmaTotalStatistics statistics{};
    if (allocator.allocator != VK_NULL_HANDLE) vmaCalculateStatistics(allocator, &statistics);
    receipt.emplace_back("gpu.allocated_bytes", std::to_string(statistics.total.statistics.allocationBytes));
    receipt.emplace_back("gi.gpu_bytes", std::to_string(giGpuBytes()));
    return receipt;
}
} // namespace vve::simple
