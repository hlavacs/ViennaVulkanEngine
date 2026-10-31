/** @file @brief Deterministic preset benchmark and opt-in retired-slot timestamp contracts; no pixel reads during measurement. */
#include <vulkan/vulkan_core.h>
#include <cstdio>
import std;
import VVEngine;
import VVEngine.Simple;
import VVEngine.Simple.Renderer;
import VVEngine.Simple.IrradianceProbes;
import VVE.TestSupport;

namespace {
/// @brief Nearest-rank percentile: median uses rank ceil(N/2), p95 uses ceil(.95*N).
double percentile(std::vector<double> values, double fraction) {
    std::ranges::sort(values);
    return values[std::size_t(std::ceil(values.size() * fraction)) - 1];
}
/// @brief Runs identical camera/material/light geometry for every backend, with bounded probe turnover after warm-up.
auto measure(std::string_view name, vve::Antialiasing aa, vve::PixelExtent extent,
    const std::filesystem::path &output, bool contactAo) -> std::optional<std::array<double,3>> {
    using namespace vve;
    auto engine = test::hiddenEngine("gi-perf", extent); auto &render = engine.renderSystem();
    const bool baseline = name == "deferred";
    if (!engine.init() || !render.setAntialiasing(aa) ||
        !render.setRendererBackend(baseline ? RendererBackend::deferred : RendererBackend::hybrid_gi) ||
        !render.setGiSettings({.ao_enabled = contactAo, .quality = name == "low" ? GiQuality::low : GiQuality::medium}) ||
        !render.setMaterialLighting(MaterialLighting::physically_based)) return {};
    render.setCamera(Camera::lookAt(Position{.value = {0,3,7}},Position{.value = {0,1,0}}));
    render.setDirectionalLight(Direction{.value = {-.4F,-1,-.3F}},LinearColor{},LightIntensity{.value = 2},LinearColor{.value = Vec3{.03F}});
    if (!render.addPlane({12,12},LinearColor{.value = Vec3{.5F}})) return {};
    // A small red/green room, visible blockers and repeated geometry exercise capture culling and contact AO.
    for (auto i : std::views::iota(0U, 12U)) {
        const Vec3 color = i % 3 == 0 ? Vec3{.7F,.05F,.03F} : i % 3 == 1 ? Vec3{.03F,.6F,.05F} : Vec3{.5F};
        if (!render.addCuboid(Vec3{-.5F,0,-.5F},Vec3{.5F,2,.5F},LinearColor{.value = color},
            Transform{.translation = {.value = {Scalar(int(i % 4) * 2 - 3),0,Scalar(int(i / 4) * 2 - 4)}}})) return {};
    }
    if (!engine.renderFrame()) return {};
    auto &gpu = render.forward(); const auto window = gpu.targets.front().handle;
    const auto &target = gpu.targets.front();
    if (target.swapchain.extent.width != extent.width || target.swapchain.extent.height != extent.height) return {};
#ifndef NDEBUG
    if (!gpu.validationActive()) return {}; // A missing validation layer must not turn this Debug GPU test into a pass.
#endif
    // Default frames must allocate no query pool and expose unavailable timing rather than invented zeros.
    auto initial = render.renderDiagnostics(window);
    if (!initial || gpu.targets.front().gpuQueries || initial->front().second != "unavailable") return {};
    const auto idle = gpu.deviceWaitIdleCount();
    render.setGpuTimingReadback(true);
    constexpr unsigned warmup = 128, samples = 30;
    std::array<std::vector<double>,5> times;
    std::uint64_t lastSerial{}, bytes{}, giBytes{};
    // Fixed 128-frame warm-up covers both 4^3 volumes even with the low preset's single-probe budget.
    for (auto frame : std::views::iota(0U, warmup + samples)) {
        if (!engine.renderFrame()) return {};
        auto receipt = render.renderDiagnostics(window);
        if (!receipt) return {};
        if (frame < warmup) continue;
        const auto serial = gpu.targets.front().timingSerial;
        const bool supported = gpu.gpuTimestampBits && gpu.gpuTimestampPeriod > 0;
        if (supported && (!(serial > lastSerial) || serial >= gpu.targets.front().submitSerials[gpu.targets.front().currentFrame ^ 1U])) return {};
        lastSerial = serial;
        for (auto i : std::views::iota(0U, 5U)) {
            const auto &value = (*receipt)[i].second;
            if (value != "unavailable") {
                const double ms = std::stod(value);
                if (!std::isfinite(ms) || ms < 0) return {};
                times[i].push_back(ms);
            } else if (supported) return {};
        }
        for (const auto &[key,value] : *receipt) {
            if (key == "gpu.allocated_bytes") bytes = std::stoull(value);
            if (key == "gi.gpu_bytes") giBytes = std::stoull(value);
        }
        // Raw samples keep outliers and the completed submission identity reviewable.
        std::println("sample.preset={} samples={} frame={} serial={} gi.capture_ms={} gi.integrate_ms={} ao.gpu_ms={} gi.lighting_ms={} gpu.frame_ms={}",
            name,aa == Antialiasing::msaa4 ? 4 : 1,frame-warmup,serial,(*receipt)[0].second,(*receipt)[1].second,
            (*receipt)[2].second,(*receipt)[3].second,(*receipt)[4].second);
    }
    VkPhysicalDeviceDriverProperties driver{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES};
    VkPhysicalDeviceProperties2 device{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, .pNext = &driver};
    vkGetPhysicalDeviceProperties2(gpu.physicalDevice.physicalDevice,&device);
    // These describe the actual capture owners/work, including zero probe storage on the deferred baseline.
    std::println("preset={} gi.probes.updated={} gi.capture.face_size={} gi.cascades={} gi.probes.captured={}",name,
        target.probes.updated+target.coarseProbes.updated,target.probes.radiance.extent.width/simple::IrradianceProbes::faces,
        unsigned(target.probes.coefficients.image != VK_NULL_HANDLE)+unsigned(target.coarseProbes.coefficients.image != VK_NULL_HANDLE),
        target.probes.valid+target.coarseProbes.valid);
    std::println("device={} driver={} vulkan={}.{}.{} resolution={}x{} samples={} preset={} warmup={} measured_frames={} gi.gpu_bytes={} gpu.allocated_bytes={} timestamp.bits={} timestamp.period_ns={} idle.delta={} ao.enabled={} validation.active={}",
        std::string_view{device.properties.deviceName}, std::string_view{driver.driverInfo}, VK_API_VERSION_MAJOR(device.properties.apiVersion),
        VK_API_VERSION_MINOR(device.properties.apiVersion),VK_API_VERSION_PATCH(device.properties.apiVersion),extent.width,extent.height,
        aa == Antialiasing::msaa4 ? 4 : 1,name,warmup,samples,giBytes,bytes,gpu.gpuTimestampBits,gpu.gpuTimestampPeriod,gpu.deviceWaitIdleCount()-idle,
        !baseline && contactAo,gpu.validationActive());
    constexpr std::array keys{"gi.capture_ms","gi.integrate_ms","ao.gpu_ms","gi.lighting_ms","gpu.frame_ms"};
    for (auto i : std::views::iota(0U,5U)) {
        if (times[i].empty()) std::println("preset={} samples={} {}.median=unavailable {}.p95=unavailable", name,aa == Antialiasing::msaa4 ? 4 : 1,keys[i],keys[i]);
        else std::println("preset={} samples={} {}.median={:.6f} {}.p95={:.6f}", name,aa == Antialiasing::msaa4 ? 4 : 1,
            keys[i],percentile(times[i],.5),keys[i],percentile(times[i],.95));
    }
    if (gpu.gpuTimestampBits && gpu.gpuTimestampPeriod > 0) {
        // Supported timestamps must measure active passes; a baseline's absent GI/AO passes have zero recorded work.
        for (auto i : std::views::iota(0U,5U))
            if (((baseline && i < 3) || (i == 2 && !contactAo)) ? percentile(times[i],.5) != 0 : percentile(times[i],.5) <= 0) return {};
    }
    if (gpu.deviceWaitIdleCount() != idle) return {};
    if (!output.empty() && !render.captureFrameToPng(output / std::format("{}-{}x.png",name,aa == Antialiasing::msaa4 ? 4 : 1))) return {};
    render.setGpuTimingReadback(false);
    if (render.renderDiagnostics(window)->front().second != "unavailable" || !engine.renderFrame()) return {};
    const auto summary = std::array{times[4].empty() ? -1. : percentile(times[4],.5),
        times[4].empty() ? -1. : percentile(times[4],.95),double(bytes)};
    render.waitIdle(); engine.gui().shutdownVulkan(); render.shutdown();
    std::println("preset={} validation.errors={}",name,gpu.validationErrorCount());
    return gpu.validationErrorCount() == 0 ? std::optional{summary} : std::nullopt;
}
}
/// @brief CTest runs compact fixtures; --measure DIR [--ao-off] runs the six cases at 1280x720 and writes PNGs.
int main(int argc, char **argv) {
    using namespace vve;
    if (std::setvbuf(stdout,nullptr,_IONBF,0) != 0) return 1; // Unbuffered receipts also work with the Windows CRT.
    const bool measuring = (argc == 3 || argc == 4) && std::string_view{argv[1]} == "--measure";
    if ((argc != 1 && !measuring) || (argc == 4 && std::string_view{argv[3]} != "--ao-off")) return 1;
    const std::filesystem::path output = measuring ? argv[2] : "";
    if (measuring) std::filesystem::create_directories(output);
    // Compile and check the forwarding facade, including invalid-window error behavior before startup.
    Engine<> facade; auto world = facade.world(); auto &api = world.get<RenderSystem>();
    api.setGpuTimingReadback(true);
    if (auto invalid = api.renderDiagnostics({}); invalid || invalid.error() != Error::invalid_handle) return 2;
    for (auto aa : {Antialiasing::none,Antialiasing::msaa4}) {
        std::optional<std::array<double,3>> baseline;
        for (std::string_view preset : {"deferred","low","medium"}) {
            const auto result = measure(preset,aa,measuring ? PixelExtent{1280,720} : PixelExtent{160,90},output,argc != 4);
            if (!result) return 2;
            if (!baseline) baseline = result;
            const bool available = (*result)[0] >= 0 && (*baseline)[0] >= 0;
            std::println("preset={} samples={} incremental.gpu_ms.median={} incremental.gpu_ms.p95={} incremental.gpu_bytes={:.0f}",
                preset,aa == Antialiasing::msaa4 ? 4 : 1,available ? std::format("{:.6f}",(*result)[0]-(*baseline)[0]) : "unavailable",
                available ? std::format("{:.6f}",(*result)[1]-(*baseline)[1]) : "unavailable",(*result)[2]-(*baseline)[2]);
        }
    }
    return 0;
}
