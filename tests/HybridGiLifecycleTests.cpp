/** @file @brief Deterministic GI ownership, failure recovery and fence retirement on real Vulkan windows. */
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_events.h>
#include <vulkan/vulkan_core.h>
import std;
import VVEngine;
import VVEngine.Simple;
import VVEngine.Simple.Renderer;
import VVE.TestSupport;

using namespace vve;
using vve::simple::ForwardRenderer;

/// @brief Names the failed invariant in CTest's small machine-readable receipt.
void require(bool condition, std::string_view name) {
    if (!condition) throw std::runtime_error{std::string{name}};
}
/// @brief Uses distant cameras to prove that both volumes, as well as AO/G-buffer images, belong to individual windows.
auto twoWindows() {
    return simple::Engine{simple::Windows{.value = {
        WindowDesc{.id = "a", .extent = {64,64}, .visible = false},
        WindowDesc{.id = "b", .extent = {96,80}, .visible = false}}}};
}
/// @brief Creates an opaque reference and deterministic cameras, without animation or post-processing.
template <typename TEngine> void start(TEngine &engine, Antialiasing aa = Antialiasing::none) {
    auto &render = engine.renderSystem();
    require(bool(engine.init()) && bool(render.setRendererBackend(RendererBackend::hybrid_gi)), "init.backend");
    require(bool(render.setAntialiasing(aa)), "init.aa");
    require(bool(render.setGiSettings(GiSettings{.ao_enabled = true})), "init.ao");
    render.setCamera(Camera::lookAt(Position{{0,2,3}}, Position{}));
    if (auto *b = engine.windowSystem().findWindow("b"))
        require(bool(render.setCamera(b->info().handle, Camera::lookAt(Position{{64,2,3}}, Position{{64,0,0}}))), "init.camera");
    require(bool(render.addCuboid(Vec3{-.5F},Vec3{.5F},LinearColor{{.7F,.2F,.1F}})), "init.object");
    if (engine.windowSystem().findWindow("b"))
        require(bool(render.addCuboid(Vec3{-.5F},Vec3{.5F},LinearColor{{.1F,.3F,.7F}},Transform{.translation = Position{{64,0,0}}})), "init.second.object");
    require(test::renderFrames(engine,4), "init.frames");
    auto &renderer = render.forward();
#ifndef NDEBUG
    require(renderer.validationActive(), "validation.required");
#endif
    VkPhysicalDeviceProperties device{}; vkGetPhysicalDeviceProperties(renderer.physicalDevice.physicalDevice,&device);
    std::println("device={} samples={} validation.active={}",std::string_view{device.deviceName},unsigned(renderer.sampleCount()),renderer.validationActive());
}
/// @brief Reports actual allocations and enforces both fixed-volume and aggregate budgets, including retired owners.
void receipt(ForwardRenderer &renderer) {
    for (const auto &target : renderer.targets) {
        const auto bytes = ForwardRenderer::giGpuBytes(target);
        require(bytes <= ForwardRenderer::giWindowByteBudget && bytes - target.aoGpuBytes <= ForwardRenderer::probeByteBudget, "budget.window");
        std::println("window={} gi.gpu_bytes={} ao.gpu_bytes={} gi.window_gpu_bytes={} gi.history_resets={} screen.extent={}x{}",
            target.handle.value,bytes-target.aoGpuBytes,target.aoGpuBytes,bytes,target.probeResets,target.hdrImage.extent.width,target.hdrImage.extent.height);
    }
    require(renderer.giGpuBytes() <= ForwardRenderer::giTotalByteBudget, "budget.total");
    std::println("gi.total_gpu_bytes={} gpu.live_allocations={} gpu.retired_owners={} device.idle_waits={}",
        renderer.giGpuBytes(),renderer.gpuAllocationCount(),renderer.retiredResourceCount(),renderer.deviceWaitIdleCount());
}
/// @brief Includes teardown in validation and checks no VMA allocations or retired owners survive the allocator's shutdown.
template <typename TEngine> void finish(TEngine &engine) {
    auto &render = engine.renderSystem(); auto &renderer = render.forward();
    receipt(renderer);
    render.waitIdle(); engine.gui().shutdownVulkan(); render.shutdown();
    std::println("validation.errors={} gpu.live_allocations={} gpu.cleanup_live_allocations={} gi.total_gpu_bytes={}",
        renderer.validationErrorCount(),renderer.gpuAllocationCount(),renderer.cleanupLiveAllocations,renderer.giGpuBytes());
    require(renderer.validationErrorCount() == 0 && renderer.gpuAllocationCount() == 0 &&
        renderer.cleanupLiveAllocations == 0 && renderer.giGpuBytes() == 0 && renderer.retiredResourceCount() == 0, "cleanup.empty");
}
/// @brief Resizes one window, exercises single/both minimized state, then teleports only that camera.
void windows(Antialiasing aa) {
    auto engine = twoWindows(); start(engine,aa);
    auto &render = engine.renderSystem(); auto &renderer = render.forward();
    auto &a = renderer.targets.front(), &b = renderer.targets.back();
    require(a.aoImages[0].image != b.aoImages[0].image && a.gbufferImages[0].image != b.gbufferImages[0].image &&
        a.probes.coefficients.image != b.probes.coefficients.image && a.probes.origin.x != b.probes.origin.x, "windows.independent");
    const auto probe = a.probes.coefficients.image, otherAo = b.aoImages[0].image;
    const auto pool = VkDescriptorPool{a.probes.pool.descriptorPool};
    const auto bExtent = b.hdrImage.extent;
    const auto aReset = a.probeResets, bReset = b.probeResets;
    auto settings = render.giSettings(); settings.enabled = false;
    require(bool(render.setGiSettings(settings)), "pause.captures");
    const auto ages = a.probes.captured;
    require(SDL_SetWindowSize(a.window,112,88) && SDL_SyncWindow(a.window), "resize.sdl");
    require(test::renderFrames(engine,2), "resize.frames");
    if (a.hdrImage.extent.width == 64) require(renderer.recreateSwapchain(a,renderer.currentWindowPixelExtent(a)) == VK_SUCCESS, "resize.hidden.recreate");
    require(a.probes.coefficients.image == probe && VkDescriptorPool{a.probes.pool.descriptorPool} == pool &&
        a.probes.captured == ages && a.probeResets == aReset && b.probeResets == bReset &&
        b.aoImages[0].image == otherAo && b.hdrImage.extent.width == bExtent.width &&
        a.aoImages[0].extent.width == a.hdrImage.extent.width && a.gbufferImages[0].extent.height == a.hdrImage.extent.height, "resize.ownership");
    // Deterministic SDL events exercise the production event handler even when a window manager ignores hidden-window minimization.
    auto *wa = engine.windowSystem().findWindow("a"), *wb = engine.windowSystem().findWindow("b");
    require(bool(engine.windowSystem().poll()), "events.drain");
    const auto minimize = [&](auto *window, bool value) {
        SDL_Event event{}; event.type = value ? SDL_EVENT_WINDOW_MINIMIZED : SDL_EVENT_WINDOW_RESTORED;
        event.window.windowID = SDL_GetWindowID(window->native());
        const bool pushed = SDL_PushEvent(&event);
        bool polled = bool(engine.windowSystem().poll());
        // SDL3 can end the prior polling cycle first after the all-minimized WaitEventTimeout; the next poll consumes restore.
        if (window->info().minimized != value) polled &= bool(engine.windowSystem().poll());
        std::println("window.event.id={} window.event.minimize={} event.pushed={} event.polled={} window.minimized={}",
            event.window.windowID,value,pushed,polled,window->info().minimized);
        require(pushed && polled && window->info().minimized == value, "minimize.event");
    };
    minimize(wa,true);
    require(test::renderFrames(engine,3) && render.lastRenderedWindowCount() == 1, "minimize.one");
    minimize(wb,true);
    const auto count = renderer.gpuAllocationCount();
    require(test::renderFrames(engine,1) && render.lastRenderedWindowCount() == 0 && renderer.gpuAllocationCount() == count, "minimize.both");
    minimize(wa,false); minimize(wb,false);
    require(test::renderFrames(engine,2) && render.lastRenderedWindowCount() == 2 && a.probes.captured == ages &&
        a.probes.coefficients.image == probe && a.probeResets == aReset, "resume.preserved");
    settings.enabled = true; require(bool(render.setGiSettings(settings)), "resume.gi");
    require(bool(render.setCamera(a.handle,Camera::lookAt(Position{{128,2,3}},Position{{128,0,0}}))) && test::renderFrames(engine,1), "teleport.frame");
    require(a.probeResets == aReset+1 && a.probes.valid == 0 && b.probeResets == bReset, "teleport.isolated.reset");
    const auto path = std::filesystem::path{VVE_GI_LIFECYCLE_REPORT_DIR} / (aa == Antialiasing::msaa4 ? "windows-msaa4.png" : "windows.png");
    std::filesystem::create_directories(path.parent_path());
    require(bool(render.captureFrameToPng(b.handle,path)), "capture.window");
    std::println("windows.independent=1 resize.preserved=1 minimize.resume=1 teleport.isolated=1 capture={}",path.string());
    finish(engine);
}
/// @brief Replaces low/medium generations without an idle boundary, including rollback after the second allocation/descriptor failure.
void quality(bool exhaustBudget) {
    auto engine = twoWindows(); start(engine);
    auto &render = engine.renderSystem(); auto &renderer = render.forward();
    const auto allocations = renderer.gpuAllocationCount(), idle = renderer.deviceWaitIdleCount();
    const auto meshes = renderer.gpuMeshUploadCount(), textures = renderer.textureUploadSubmitCount();
    const auto image = renderer.targets.front().aoImages[0].image;
    auto settings = render.giSettings();
    // Fail in window two's probes, then in window two's new deferred descriptors after window one's exchange succeeded.
    for (const auto checkpoint : {13U,27U}) {
        const auto previous = renderer.targets.front().probes.coefficients.image;
        renderer.debugGiFailAfter = checkpoint; settings.quality = GiQuality::low;
        const auto failed = render.setGiSettings(settings);
        require(!failed && render.giSettings().quality == GiQuality::medium && renderer.gpuAllocationCount() == allocations &&
            renderer.targets.front().probes.coefficients.image == previous && renderer.retiredResourceCount() == 0, "quality.rollback");
    }
    std::uint64_t peak{}; unsigned changes{};
    const auto limit = exhaustBudget ? 64U : 3U;
    for (auto i : std::views::iota(0U,limit)) {
        const auto resets = renderer.targets.front().probeResets;
        settings.quality = render.giSettings().quality == GiQuality::low ? GiQuality::medium : GiQuality::low;
        const auto selected = render.setGiSettings(settings);
        if (!selected) {
            require(exhaustBudget && selected.error() == Error::capacity_exceeded && render.giSettings().quality != settings.quality, "budget.transactional");
            break;
        }
        ++changes; peak = std::max(peak,renderer.giGpuBytes()); receipt(renderer);
        require(renderer.targets.front().aoImages[0].image == image && renderer.gpuAllocationCount() > allocations, "quality.retired");
        if (!exhaustBudget) require(test::renderFrames(engine,3) && renderer.targets.front().probeResets == resets+1 &&
            renderer.targets.back().probeResets == resets+1, "quality.frames.reset");
    }
    require(changes >= 3 && (!exhaustBudget || changes < limit), "quality.transitions");
    require(test::renderFrames(engine,4) && renderer.gpuAllocationCount() == allocations && renderer.retiredResourceCount() == 0 &&
        renderer.deviceWaitIdleCount() == idle && renderer.gpuMeshUploadCount() == meshes &&
        renderer.textureUploadSubmitCount() == textures, "quality.fence.reclaimed");
    std::println("quality.changes={} gi.peak_gpu_bytes={} gi.total_budget={} quality.idle_delta={}",changes,peak,ForwardRenderer::giTotalByteBudget,renderer.deviceWaitIdleCount()-idle);
    finish(engine);
}
/// @brief Five independent engines must have identical steady allocations and release them all before the next engine.
void cycles() {
    std::uint64_t baseline{};
    for (auto cycle : std::views::iota(0U,5U)) {
        auto engine = test::hiddenEngine("gi-cycle",PixelExtent{64,64}); start(engine);
        const auto count = engine.renderSystem().forward().gpuAllocationCount();
        if (cycle == 0) baseline = count;
        require(count == baseline, "cycles.stable.allocations");
        std::println("cycle={} gpu.live_allocations={} gpu.baseline_allocations={}",cycle,count,baseline);
        finish(engine);
    }
}
/// @brief Exercises actual partial probe/AO/descriptor allocation, plus a second-window failure, before clean initialization retry.
void failure() {
    for (auto checkpoint : {0U,3U,6U,12U,13U,15U,16U}) {
        auto engine = twoWindows(); auto &render = engine.renderSystem(); auto &renderer = render.forward();
        require(bool(engine.init()) && bool(render.setRendererBackend(RendererBackend::hybrid_gi)), "failure.setup");
        renderer.debugGiFailAfter = checkpoint;
        const auto failed = engine.renderFrame();
        require(!failed && failed.error() == Error::platform_error && !renderer.initialized() && renderer.targets.empty() &&
            renderer.gpuAllocationCount() == 0 && renderer.cleanupLiveAllocations == 0 && renderer.validationErrorCount() == 0, "failure.released");
        std::println("failure.checkpoint={} failure.live_allocations={} validation.errors={}",checkpoint,renderer.gpuAllocationCount(),renderer.validationErrorCount());
        require(test::renderFrames(engine,3), "failure.retry");
#ifndef NDEBUG
        require(renderer.validationActive(), "failure.validation.required");
#endif
        finish(engine);
    }
}
/// @brief Scene changes reset even with unrelated objects; native owners retire safely during replacement, clearing and resize.
void sceneAndNative() {
    auto engine = twoWindows(); start(engine);
    auto &render = engine.renderSystem(); auto &renderer = render.forward();
    auto &a = renderer.targets.front(), &b = renderer.targets.back();
    const auto path = test::writeFixture(VVE_GI_LIFECYCLE_REPORT_DIR,"scene.obj",test::triangleObj);
    const auto scene = engine.assets().loadScene(path); require(bool(scene), "scene.load");
    auto resets = a.probeResets;
    const auto instance = render.instantiateScene(*scene); require(bool(instance) && test::renderFrames(engine,1), "scene.instantiate");
    require(a.probeResets == resets+1 && b.probeResets == a.probeResets && a.probes.valid == 0, "scene.instantiate.reset");
    resets = a.probeResets;
    require(bool(render.removeSceneInstance(*instance)) && test::renderFrames(engine,1) && a.probeResets == resets+1, "scene.instance.remove.reset");
    resets = a.probeResets;
    require(bool(render.removeScene(*scene)) && test::renderFrames(engine,1) && a.probeResets == resets+1, "scene.remove.reset");
    resets = a.probeResets; render.clearScene();
    require(test::renderFrames(engine,1) && a.probeResets == resets+1 && a.probes.valid == 0, "scene.clear.reset");
    // A shared application owner stands in for procedural GPU resources; the weak pointer observes actual fence retirement.
    auto owner = std::make_shared<unsigned>(0); std::weak_ptr<unsigned> previous = owner;
    require(bool(render.setNativeDraw([owner](const NativeDrawContext &) -> std::expected<void,Error> { ++*owner; return {}; })), "native.register");
    owner.reset(); require(test::renderFrames(engine,2), "native.draw");
    const auto idle = renderer.deviceWaitIdleCount();
    unsigned calls{};
    require(bool(render.setNativeDraw([&](const NativeDrawContext &) -> std::expected<void,Error> { ++calls; return {}; })) && !previous.expired(), "native.pending");
    auto settings = render.giSettings(); settings.quality = GiQuality::low;
    require(bool(render.setGiSettings(settings)) && test::renderFrames(engine,3) && previous.expired() && calls > 0, "native.quality.retired");
    require(bool(render.setNativeDraw({})) && test::renderFrames(engine,3) && renderer.deviceWaitIdleCount() == idle, "native.clear");
    std::println("scene.reset_events={} native.calls={} native.retired=1",a.probeResets-resets,calls);
    finish(engine);
}
/// @brief Each registered case fails on missing rendering or validation; no GPU case is treated as a successful skip.
int main(int argc, char **argv) {
    try {
        const std::string_view mode = argc > 1 ? argv[1] : "windows";
        if (mode == "windows" || mode == "msaa4") windows(mode == "msaa4" ? Antialiasing::msaa4 : Antialiasing::none);
        else if (mode == "quality" || mode == "budget") quality(mode == "budget");
        else if (mode == "cycles") cycles();
        else if (mode == "failure") failure();
        else if (mode == "scene-native") sceneAndNative();
        else return 1;
        std::println("lifecycle.mode={} lifecycle.pass=1",mode); return 0;
    } catch (const std::exception &error) { std::println("lifecycle.pass=0 invariant={}",error.what()); return 2; }
}
