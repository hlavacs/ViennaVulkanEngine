/** @file @brief HDR wall-leak rejection and GPU inside-probe validity, with explicit fallback and finite-value checks. */
#include <vulkan/vulkan_core.h>
import std;
import VVEngine;
import VVEngine.Simple;
import VVEngine.Simple.Renderer;
import VVEngine.Simple.IrradianceProbes;
import VVEngine.Simple.Vulkan;
import VVE.TestSupport;

namespace {
/// @brief Overwrites every probe twice, using the ordinary bounded GPU capture budget.
bool warm(vve::simple::Engine &engine, unsigned frames = 64) {
    for (auto frame : std::views::iota(0U, frames)) { if (!engine.renderFrame()) return false; }
    return true;
}
/// @brief Projects a known receiver point and measures a four-pixel HDR patch entirely on its neutral surface.
std::optional<vve::Vec3> measure(vve::simple::Engine &engine, vve::Camera camera, vve::Vec3 point, std::string_view state) {
    using namespace vve;
    auto &renderer = engine.renderSystem().forward();
    const auto clip = math::multiply(math::multiply(math::perspectiveVulkan(camera.fov_y.radians, 1, .1F, 100),
        math::lookAt(camera.position.value, math::add(camera.position.value, camera.forward.value), Vec3{0,1,0})), Vec4{point.x,point.y,point.z,1});
    const auto x = static_cast<unsigned>((clip.x / clip.w * .5F + .5F) * 128) - 2;
    const auto y = static_cast<unsigned>((clip.y / clip.w * .5F + .5F) * 128) - 2;
    const auto value = test::linearRegion(renderer, x, y, 4, 4);
    if (value && (!std::isfinite(value->x) || !std::isfinite(value->y) || !std::isfinite(value->z))) return {};
    if (value) std::println("state={} region={},{},4,4 receiver.linear={:.7f},{:.7f},{:.7f}", state,x,y,value->x,value->y,value->z);
    return value;
}
/// @brief A neutral floor has zero direct cosine to the horizontal light; only the red wall can change its hue.
bool leak(vve::simple::Engine &engine, const std::filesystem::path &directory) {
    using namespace vve;
    auto &render = engine.renderSystem();
    auto &renderer = render.forward();
    render.setBackgroundColor(LinearColor{.value = Vec3{0}});
    render.setDirectionalLight(Direction{.value = Vec3{-1,0,0}},LinearColor{},LightIntensity{4},LinearColor{.value = Vec3{0}});
    const auto floor = render.addPlane(Vec2{8,8},LinearColor{.value = Vec3{.5F}});
    const auto wall = render.addCuboid(Vec3{-1.1F,-3,-5},Vec3{-.9F,4,5},LinearColor{.value = Vec3{0}});
    const auto contributor = render.addCuboid(Vec3{-3.2F,0,-4},Vec3{-3,3,4},LinearColor{.value = Vec3{1,.01F,.01F}});
    if (!floor || !wall || !contributor) return false;
    const Vec3 protectedPoint{-.5F,0,1};
    const auto protectedCamera = Camera::lookAt(Position{.value = Vec3{.2F,1.5F,3}},Position{.value = protectedPoint}, {}, FovY{1.1F});
    render.setCamera(protectedCamera);
    renderer.debugProbeVisibility = false;
    if (!warm(engine)) return false;
    const auto baseline = measure(engine,protectedCamera,protectedPoint,"protected-visibility-disabled");
    if (!render.captureFrameToPng(directory / "leak-baseline.png")) return false;
    renderer.debugProbeVisibility = true;
    if (!warm(engine,2)) return false;
    const auto visible = measure(engine,protectedCamera,protectedPoint,"protected-visibility");
    if (!render.captureFrameToPng(directory / "leak-visibility.png")) return false;
    const Vec3 openPoint{-1.6F,0,1};
    const auto openCamera = Camera::lookAt(Position{.value = Vec3{-1.7F,1.5F,3}},Position{.value = openPoint}, {}, FovY{1.1F});
    render.setCamera(openCamera);
    if (!warm(engine)) return false;
    const auto open = measure(engine,openCamera,openPoint,"unprotected-red");
    if (!render.captureFrameToPng(directory / "unprotected-red.png") ||
        !render.setObjectColor(*contributor,LinearColor{.value = Vec3{0}}) || !warm(engine)) return false;
    const auto openZero = measure(engine,openCamera,openPoint,"unprotected-no-contributor");
    render.setCamera(protectedCamera);
    if (!warm(engine)) return false;
    const auto zero = measure(engine,protectedCamera,protectedPoint,"protected-no-contributor");
    renderer.debugProbeVisibility = false;
    if (!warm(engine,2)) return false;
    const auto baselineZero = measure(engine,protectedCamera,protectedPoint,"protected-no-contributor-visibility-disabled");
    renderer.debugProbeVisibility = true;
    const auto counts = test::probeValidity(renderer);
    if (!baseline || !visible || !open || !openZero || !zero || !baselineZero || !counts) return false;
    const float leaked = baseline->x - baselineZero->x, protectedBounce = visible->x - zero->x;
    const float openBounce = open->x - openZero->x;
    std::println("leak.baseline.red={} leak.visibility.red={} leak.retained_fraction={} leak.limit=0.30 unprotected.bounce.red={} unprotected.limit=0.005",
        leaked,protectedBounce,protectedBounce / std::max(leaked,.000001F),openBounce);
    // Isolated cross-wall probes supply the baseline. Require >70% rejection, while the open receiver keeps real color transport.
    return leaked > .005F && protectedBounce < .30F * leaked && openBounce > .005F && open->x > 1.5F * open->y &&
        counts->first + counts->second == simple::IrradianceProbes::count;
}
/// @brief Four probes lie strictly inside a black block. Their missing shares must retain the uniform environment fallback.
bool inside(vve::simple::Engine &engine, const std::filesystem::path &directory) {
    using namespace vve;
    auto &render = engine.renderSystem();
    render.setBackgroundColor(LinearColor{.value = Vec3{.2F}});
    render.setPointLight(Position{},LinearColor{},LightIntensity{0},LightRange{1},LinearColor{.value = Vec3{.2F}});
    render.clearLights();
    const Vec3 point{-.9F,0,1};
    const auto camera = Camera::lookAt(Position{.value = Vec3{0,1.5F,3}},Position{.value = point}, {}, FovY{1.1F});
    render.setCamera(camera);
    const auto floor = render.addPlane(Vec2{.8F,.8F},LinearColor{.value = Vec3{.5F}},Transform{.translation = Position{.value = point}});
    const auto block = render.addCuboid(Vec3{-2.4F,-.4F,-.4F},Vec3{-1.6F,2.4F,2.4F},LinearColor{.value = Vec3{0}});
    if (!floor || !block) return false;
    auto settings = render.giSettings(); settings.enabled = false;
    if (!render.setGiSettings(settings) || !warm(engine,2)) return false;
    const auto off = measure(engine,camera,point,"inside-gi-off-fallback");
    settings.enabled = true;
    if (!render.setGiSettings(settings) || !warm(engine)) return false;
    const auto on = measure(engine,camera,point,"inside-gi-on");
    if (!render.captureFrameToPng(directory / "inside.png")) return false;
    const auto counts = test::probeValidity(render.forward());
    if (!off || !on || !counts) return false;
    // The black block physically occludes part of the remaining probes' sky. 0.02 permits that local effect;
    // dropping the inactive corners' 45% ambient share would lose 0.045, well beyond this tolerance.
    constexpr float tolerance = .02F;
    std::println("inside.fallback={} inside.receiver={} inside.tolerance={} inside.finite={}",off->x,on->x,tolerance,
        std::isfinite(on->x) && std::isfinite(on->y) && std::isfinite(on->z));
    return counts->second >= 1 && counts->first + counts->second == simple::IrradianceProbes::count && off->x > .09F &&
        std::isfinite(on->x) && std::isfinite(on->y) && std::isfinite(on->z) &&
        std::min({on->x,on->y,on->z}) >= off->x - tolerance && std::max({on->x,on->y,on->z}) <= off->x + tolerance;
}
}
/// @brief Uses the actual Vulkan device and requires Debug validation through resource teardown.
int main(int argc, char **argv) {
    using namespace vve;
    const bool insideGeometry = argc == 2 && std::string_view{argv[1]} == "--inside";
    if (argc > 1 && !insideGeometry) return 3;
    auto engine = test::hiddenEngine("probe-visibility",PixelExtent{128,128});
    auto &render = engine.renderSystem();
    if (!engine.init() || !render.setRendererBackend(RendererBackend::hybrid_gi) || !render.setAnimationTime(AnimationTime{})) return 1;
    const auto directory = std::filesystem::path{VVE_TEST_TMP_DIR};
    std::filesystem::create_directories(directory);
    bool pass = insideGeometry ? inside(engine,directory) : leak(engine,directory);
    auto &renderer = render.forward();
    VkPhysicalDeviceProperties device{}; vkGetPhysicalDeviceProperties(renderer.physicalDevice.physicalDevice,&device);
    const bool validation = renderer.validationActive();
    render.waitIdle(); engine.gui().shutdownVulkan(); render.shutdown();
    std::println("device={} validation_active={} validation.errors={} captures={}",std::string_view{device.deviceName},validation,renderer.validationErrorCount(),directory.string());
#ifndef NDEBUG
    pass &= validation;
#endif
    return pass && renderer.validationErrorCount() == 0 ? 0 : 2;
}
