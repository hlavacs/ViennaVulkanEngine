/** @file @brief Native probe receipts and facade-only runtime debug views, measured in deterministic linear HDR. */
#include <vulkan/vulkan_core.h>
import std;
import VVEngine;
import VVEngine.Simple;
import VVEngine.Simple.Renderer;
import VVEngine.Simple.IrradianceProbes;
import VVEngine.Simple.Vulkan;
import VVE.TestSupport;

namespace {
using namespace vve;
constexpr PixelExtent extent{128,128}; ///< Small deterministic graphics fixture; no display-space thresholds.
/// @brief Locates a mandatory receipt key; missing keys fail the test instead of silently becoming zero.
std::string value(const Vector<std::pair<std::string,std::string>> &receipt, std::string_view key) {
    const auto found = std::ranges::find(receipt,key,&std::pair<std::string,std::string>::first);
    if (found == receipt.end()) throw std::runtime_error{std::string{key}};
    return found->second;
}
/// @brief Creates a floor and a closed black block enclosing known near-grid probes, without animation or external assets.
template <typename TRender> bool scene(TRender &render) {
    render.setBackgroundColor(LinearColor{.value = Vec3{.2F}});
    render.clearLights();
    return bool(render.addPlane({40,40},LinearColor{.value = Vec3{.5F}})) &&
        bool(render.addCuboid({-2.4F,-.4F,-.4F},{-1.6F,2.4F,2.4F},LinearColor{.value = Vec3{0}}));
}
/// @brief Compares the public snapshot to the independent support module's complete atlas readback at explicit checkpoints.
bool receipts() {
    auto engine = test::hiddenEngine("gi-diagnostics",extent); auto &render = engine.renderSystem();
    if (!engine.init() || !render.setRendererBackend(RendererBackend::hybrid_gi) || !scene(render)) return false;
    render.setCamera(Camera::lookAt(Position{.value = {0,3,5}},Position{}));
    if (!engine.renderFrame()) return false;
    auto &gpu = render.forward(); auto &target = gpu.targets.front(); const auto window = target.handle;
#ifndef NDEBUG
    if (!gpu.validationActive()) return false;
#endif
    const auto idle = gpu.deviceWaitIdleCount(), uploads = gpu.gpuMeshUploadCount();
    // Initial, full, paused and teleported snapshots cover uncaptured, inactive, active and reset rows in both volumes.
    for (auto checkpoint : std::views::iota(0U,4U)) {
        if (checkpoint == 1) {
            render.setGpuTimingReadback(true);
            if (!test::renderFrames(engine,64)) return false;
        } else if (checkpoint == 2) {
            auto settings = render.giSettings(); settings.enabled = false;
            if (!render.setGiSettings(settings) || !test::renderFrames(engine,3)) return false;
        } else if (checkpoint == 3) {
            render.setCamera(Camera::lookAt(Position{.value = {40,3,5}},Position{.value = {40,0,0}}));
            if (!engine.renderFrame()) return false;
        }
        const auto near = test::probeValidity(gpu), coarse = test::probeValidity(gpu,1);
        const auto receipt = render.renderDiagnostics(window);
        if (!near || !coarse || !receipt) return false;
        for (const auto &[key,entry] : *receipt) std::println("checkpoint={} {}={}",checkpoint,key,entry);
        const auto active = near->first + coarse->first, inactive = near->second + coarse->second;
        const auto total = 2 * simple::IrradianceProbes::count;
        if (std::stoul(value(*receipt,"gi.probes.valid")) != active ||
            std::stoul(value(*receipt,"gi.probes.inactive")) != inactive ||
            std::stoul(value(*receipt,"gi.probes.total")) != total ||
            std::stoul(value(*receipt,"gi.probes.updated")) != (checkpoint < 2 ? 2U : 0U) ||
            std::stoull(value(*receipt,"gi.history_resets")) != target.probeResets ||
            std::stoull(value(*receipt,"gi.gpu_bytes")) != gpu.giGpuBytes() ||
            std::abs(std::stod(value(*receipt,"gi.coverage.invalid"))-double(total-active)/total) > .000001) return false;
        if (checkpoint == 1 && (inactive == 0 || active == 0 || active + inactive != total)) return false;
        if (checkpoint == 3 && (active != 0 || inactive != 0)) return false;
        // Check the public transport snapshot against independently read GPU coefficients and scheduler freshness.
        for (auto cascade : std::views::iota(0U,2U)) {
            const auto &p = cascade == 0 ? target.probes : target.coarseProbes;
            const auto prefix = std::format("gi.cascade.{}",cascade);
            if (value(*receipt,prefix + ".origin") != std::format("{},{},{}",p.origin.x,p.origin.y,p.origin.z) ||
                std::stof(value(*receipt,prefix + ".spacing")) != p.scale * simple::IrradianceProbes::spacing ||
                std::stoul(value(*receipt,prefix + ".stale")) != p.stale) return false;
            simple::VulkanReadback readback;
            if (readback.create(gpu.allocator,gpu.device.device,gpu.device.graphicsQueue,gpu.commandPool.commandPool,
                p.coefficients.extent,VK_FORMAT_R32G32B32A32_SFLOAT) != VK_SUCCESS ||
                readback.capture(p.coefficients.image,0,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) != VK_SUCCESS) return false;
            for (auto row : std::views::iota(0U,simple::IrradianceProbes::count)) {
                const auto probe = std::format("{}.probe.{}",prefix,row);
                if (std::stoull(value(*receipt,probe + ".generation")) != p.generation[row]) return false;
                for (auto c : std::views::iota(0U,simple::IrradianceProbes::shCoefficients)) {
                    std::array<float,4> sh{};
                    std::memcpy(sh.data(),readback.pixelBytes().data() + (row * simple::IrradianceProbes::atlasColumns + c) * sizeof(sh),sizeof(sh));
                    if (value(*receipt,std::format("{}.sh.{}",probe,c)) != std::format("{},{},{},{}",sh[0],sh[1],sh[2],sh[3])) return false;
                }
            }
        }
        if (std::stoull(value(*receipt,"gi.shadow.renders")) != target.probeShadowRenders) return false;
        const bool supported = gpu.gpuTimestampBits && gpu.gpuTimestampPeriod > 0;
        // Disabled timing is explicitly unavailable. Supported retired slots provide real finite, nonnegative intervals.
        for (auto key : {"gi.capture_ms","gi.integrate_ms","gi.lighting_ms","ao.gpu_ms"}) {
            const auto entry = value(*receipt,key);
            if ((checkpoint == 0 || !supported) ? entry != "unavailable" : entry == "unavailable") return false;
            if (entry != "unavailable" && (!std::isfinite(std::stod(entry)) || std::stod(entry) < 0)) return false;
            if (checkpoint == 1 && supported && key != std::string_view{"ao.gpu_ms"} && std::stod(entry) <= 0) return false;
        }
    }
    render.setGpuTimingReadback(false);
    if (value(*render.renderDiagnostics(window),"gi.capture_ms") != "unavailable" ||
        gpu.deviceWaitIdleCount() != idle || gpu.gpuMeshUploadCount() != uploads) return false;
    render.waitIdle(); engine.gui().shutdownVulkan(); render.shutdown();
    std::println("validation.errors={}",gpu.validationErrorCount());
    return gpu.validationErrorCount() == 0;
}
/// @brief Measures a two-square-pixel patch projected from a known world-space surface point through public camera math.
std::optional<Vec3> point(const Vector<Vec4> &pixels, Camera camera, Vec3 position) {
    const auto clip = math::multiply(math::multiply(math::perspectiveVulkan(camera.fov_y.radians,1,camera.clip.near_plane,camera.clip.far_plane),
        math::lookAt(camera.position.value,math::add(camera.position.value,camera.forward.value),Vec3{0,1,0})),Vec4{position,1});
    const int x = int((clip.x/clip.w*.5F+.5F)*extent.width)-1, y = int((clip.y/clip.w*.5F+.5F)*extent.height)-1;
    if (x < 0 || y < 0 || x+2 > int(extent.width) || y+2 > int(extent.height)) return {};
    Vec3 sum{};
    for (int row : {y,y+1}) for (int column : {x,x+1}) sum = math::add(sum,Vec3{pixels[row*extent.width+column]});
    const auto result = math::scale(sum,.25F);
    std::println("point={},{},{} rect={},{},2,2 linear={},{},{}",position.x,position.y,position.z,x,y,result.x,result.y,result.z);
    return result;
}
/// @brief All authoring, settings, rendering and HDR capture for debug tests use only public facade contracts.
bool view(std::string_view mode, Antialiasing aa) {
    Engine<> engine{WindowSetups{WindowSetup{}.id("main").extent(extent).visible(false)}};
    auto world = engine.world(); auto &render = world.get<RenderSystem>();
    auto camera = Camera::lookAt(Position{.value = {0,3,5}},Position{});
    if (!engine.init() || !render.setAntialiasing(aa) || !render.setRendererBackend(RendererBackend::hybrid_gi) || !scene(render)) return false;
    render.setCamera(camera);
    // A complete deterministic sweep populates both cascades, including enclosed probes.
    for ([[maybe_unused]] auto frame : std::views::iota(0U,65U)) if (!engine.step()) return false;
#ifndef NDEBUG
    if (!render.validationActive()) return false;
#endif
    const auto windows = world.get<WindowSystem>().windows();
    const auto window = windows.front().handle();
    auto settings = render.giSettings(); settings.enabled = false; // Freeze the measured atlas across every view switch.
    if (!render.setGiSettings(settings) || !engine.step()) return false;
    const auto baseline = render.captureFrameLinear(window);
    if (!baseline) return false;
    constexpr std::array names{"none","indirect_only","ao","probe_validity","gbuffer_normals","gbuffer_depth"};
    const auto selected = std::ranges::find(names,mode);
    if (selected == names.end()) return false;
    settings.debug_view = static_cast<GiDebugView>(selected-names.begin());
    // Reject unknown values at runtime without changing any retained control or the current image.
    for (int badValue : {-1,99}) {
        auto bad = settings; bad.debug_view = static_cast<GiDebugView>(badValue); bad.indirect_strength = 3; bad.ao_enabled = true;
        const auto rejected = render.setGiSettings(bad); const auto retained = render.giSettings();
        if (rejected || rejected.error() != Error::invalid_argument || retained.debug_view != GiDebugView::none ||
            retained.enabled != false || retained.ao_enabled || retained.indirect_strength != 1 || retained.quality != settings.quality) return false;
    }
    if (!render.setGiSettings(settings) || render.giSettings().debug_view != settings.debug_view || !engine.step()) return false;
    auto pixels = render.captureFrameLinear(window);
    if (!pixels) return false;
    // Optional visual evidence stays in the caller's transient report directory; CTest needs only native HDR assertions.
    if (const char *directory = std::getenv("VVE_GI_DIAGNOSTICS_CAPTURE_DIR")) {
        std::filesystem::create_directories(directory);
        if (!render.captureFrameToPng(std::filesystem::path{directory}/std::format("{}-{}x.png",mode,aa == Antialiasing::msaa4 ? 4 : 1))) return false;
    }
    bool pass = true;
    const auto sample = point(*pixels,camera,{1,0,0});
    if (!sample) return false;
    // 0.001 permits a few FP16 steps; channel encodings of 0/0.5/1 and explicit black/white are exact.
    if (mode == "none") {
        pass = std::memcmp(pixels->data(),baseline->data(),pixels->size()*sizeof(Vec4)) == 0;
        settings.debug_view = GiDebugView::gbuffer_normals;
        if (!render.setGiSettings(settings) || !engine.step()) return false;
    }
    if (mode == "indirect_only") {
        pass = test::rgbDelta(*sample,Vec3{}) == 0;
        settings.enabled = true; settings.indirect_strength = 0;
        if (!render.setGiSettings(settings) || !engine.step()) return false;
        pixels = render.captureFrameLinear(window);
        pass &= pixels && std::ranges::all_of(*pixels,[](auto p) { return p.x == 0 && p.y == 0 && p.z == 0; });
        // Active GI must show actual sky irradiance, not an always-black diagnostic stub.
        settings.indirect_strength = 1;
        if (!render.setGiSettings(settings) || !engine.step()) return false;
        pixels = render.captureFrameLinear(window);
        const auto active = pixels ? point(*pixels,camera,{1,0,0}) : std::nullopt;
        pass &= active && active->x > .005F;
    }
    if (mode == "ao") {
        pass = std::ranges::all_of(*pixels,[](auto p) { return p.x == 1 && p.y == 1 && p.z == 1; });
        settings.ao_enabled = true;
        if (!render.setGiSettings(settings) || !engine.step()) return false;
        pixels = render.captureFrameLinear(window);
        const auto contact = pixels ? point(*pixels,camera,{-1.45F,0,1}) : std::nullopt;
        const auto open = pixels ? point(*pixels,camera,{1,0,0}) : std::nullopt;
        pass &= contact && open && contact->x < .99F && open->x > .99F && contact->x == contact->y && contact->y == contact->z;
    }
    if (mode == "gbuffer_normals") pass = test::rgbDelta(*sample,Vec3{.5F,1,.5F}) == 0;
    if (mode == "gbuffer_depth") {
        const auto original = sample->x;
        camera.position.value = math::subtract(camera.position.value,math::scale(math::normalize(camera.forward.value),4)); render.setCamera(camera);
        if (!engine.step()) return false;
        pixels = render.captureFrameLinear(window);
        const auto farther = pixels ? point(*pixels,camera,{1,0,0}) : std::nullopt;
        const auto viewPos = math::multiply(math::lookAt(camera.position.value,math::add(camera.position.value,camera.forward.value),Vec3{0,1,0}),Vec4{1,0,0,1});
        pass = farther && farther->x > original + .03F && std::abs(farther->x + viewPos.z/camera.clip.far_plane) < .001F &&
            farther->x == farther->y && farther->y == farther->z;
    }
    if (mode == "probe_validity") {
        const auto inactive = point(*pixels,camera,{-2,2.4F,1});
        pass = test::rgbDelta(*sample,Vec3{0,1,0}) == 0 && inactive && test::rgbDelta(*inactive,Vec3{1,0,0}) == 0;
        // Fresh quality storage with captures paused is blue; outside both bounded grids is black.
        settings.quality = GiQuality::low;
        if (!render.setGiSettings(settings) || !engine.step()) return false;
        pixels = render.captureFrameLinear(window);
        const auto untouched = pixels ? point(*pixels,camera,{1,0,0}) : std::nullopt;
        pass &= untouched && test::rgbDelta(*untouched,Vec3{0,0,1}) == 0;
        // The outside point may be off-screen; use a camera facing it without moving the anchored volume.
        camera = Camera::lookAt(camera.position,Position{.value = {14,0,0}}); render.setCamera(camera);
        if (!engine.step()) return false;
        pixels = render.captureFrameLinear(window);
        const auto uncovered = pixels ? point(*pixels,camera,{14,0,0}) : std::nullopt;
        pass &= uncovered && test::rgbDelta(*uncovered,Vec3{}) == 0;
    }
    // Restoring none after every diagnostic must reproduce the complete default HDR image byte for byte.
    render.setCamera(Camera::lookAt(Position{.value = {0,3,5}},Position{}));
    if (!render.setGiSettings(GiSettings{.enabled = false}) || !engine.step()) return false;
    const auto restored = render.captureFrameLinear(window);
    const bool identical = restored && restored->size() == baseline->size() &&
        std::memcmp(restored->data(),baseline->data(),baseline->size()*sizeof(Vec4)) == 0;
    pass &= identical;
    std::println("debug.view={} samples={} passed={} none.identical={} validation.errors={}",mode,aa == Antialiasing::msaa4 ? 4 : 1,pass,identical,render.validationErrorCount());
    return pass && render.validationErrorCount() == 0;
}
}
/// @brief Separate CTest modes identify each failed contract; the default case independently verifies native receipts.
int main(int argc, char **argv) {
    try {
        if (argc == 1) return receipts() ? 0 : 2;
        if (argc > 3 || (argc == 3 && std::string_view{argv[2]} != "--msaa4")) return 3;
        return view(argv[1],argc == 3 ? vve::Antialiasing::msaa4 : vve::Antialiasing::none) ? 0 : 2;
    } catch (const std::exception &error) { std::println("diagnostics.error={}",error.what()); return 2; }
}
