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
/// @brief Copies retired-slot timings and allocation counters; only an explicit probe snapshot transfers atlas metadata.
auto ForwardRenderer::renderDiagnostics(WindowHandle window, bool probeSnapshot) const -> std::expected<Vector<std::pair<std::string,std::string>>,Error> {
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
    receipt.emplace_back("gi.generation",std::to_string(probeGeneration_));
    receipt.emplace_back("gi.shadow.renders",std::to_string(target->probeShadowRenders));
    receipt.emplace_back("gi.probes.updated",std::to_string(target->probesUpdated));
    receipt.emplace_back("gi.history_resets",std::to_string(target->probeResets));
    if (!probeSnapshot) return receipt; // Benchmark sampling must not serialize the queue with diagnostic transfers.
    unsigned valid{}, inactive{}, total{};
    // Copy four SH coefficients and the first moment: five RGBA32F columns, 64 rows per initialized cascade (10 KiB total).
    for (const auto *p : {&target->probes, &target->coarseProbes}) {
        if (p->coefficients.image == VK_NULL_HANDLE) continue;
        const auto prefix = std::format("gi.cascade.{}",p == &target->probes ? 0 : 1);
        receipt.emplace_back(prefix + ".origin",std::format("{},{},{}",p->origin.x,p->origin.y,p->origin.z));
        receipt.emplace_back(prefix + ".spacing",std::to_string(p->scale * IrradianceProbes::spacing));
        receipt.emplace_back(prefix + ".stale",std::to_string(p->stale));
        total += IrradianceProbes::count;
        if (!p->initialized) continue; // Never transition or interpret an atlas that has not been submitted.
        constexpr VkExtent2D extent{IrradianceProbes::shCoefficients + 1, IrradianceProbes::count};
        VulkanReadback readback;
        if (readback.create(allocator,device.device,device.graphicsQueue,commandPool.commandPool,extent,VK_FORMAT_R32G32B32A32_SFLOAT) != VK_SUCCESS ||
            readback.capture(p->coefficients.image,0,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) != VK_SUCCESS) return std::unexpected(Error::platform_error);
        // Signed-distance integration marks active rows in SH alpha; inactive captures retain positive radial moments.
        for (auto row : std::views::iota(0U,extent.height)) {
            std::array<float,4> sh{}, moment{};
            const auto *bytes = readback.pixelBytes().data() + row * extent.width * sizeof(sh);
            std::memcpy(sh.data(),bytes,sizeof(sh));
            std::memcpy(moment.data(),bytes + IrradianceProbes::shCoefficients * sizeof(sh),sizeof(moment));
            valid += sh[3] > .5F;
            inactive += sh[3] == 0 && moment[0] > 0;
            // Reuse the existing bounded transfer to expose transport and refresh evidence by toroidal storage row.
            const auto probe = std::format("{}.probe.{}",prefix,row);
            receipt.emplace_back(probe + ".generation",std::to_string(p->generation[row]));
            for (auto coefficient : std::views::iota(0U,IrradianceProbes::shCoefficients)) {
                std::memcpy(sh.data(),bytes + coefficient * sizeof(sh),sizeof(sh));
                receipt.emplace_back(std::format("{}.sh.{}",probe,coefficient),std::format("{},{},{},{}",sh[0],sh[1],sh[2],sh[3]));
            }
        }
    }
    receipt.emplace_back("gi.probes.valid",std::to_string(valid));
    receipt.emplace_back("gi.probes.inactive",std::to_string(inactive));
    // Invalid coverage is the fraction of allocated rows without active validity (inactive plus uncaptured), not screen pixels.
    receipt.emplace_back("gi.coverage.invalid",std::format("{:.6f}",total ? double(total-valid)/total : 0.));
    receipt.emplace_back("gi.probes.total",std::to_string(total));
    return receipt;
}
} // namespace vve::simple
