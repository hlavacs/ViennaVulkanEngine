/** @file @brief Real raster-probe color transport, off-camera capture, removal and feedback-free convergence. */
#include <vulkan/vulkan_core.h>
import std;
import VVEngine;
import VVEngine.Simple;
import VVEngine.Simple.Renderer;
import VVEngine.Simple.IrradianceProbes;
import VVE.TestSupport;

namespace {
/// @brief Runs enough frames for every probe to be overwritten twice, with paused weather/material animation.
bool warm(vve::simple::Engine &engine, unsigned frames = 64) {
    for (auto frame : std::views::iota(0U,frames)) { if (!engine.renderFrame()) return false; }
    return true;
}
/// @brief Reports measured receiver radiance; HDR readback occurs only at these explicit diagnostic checkpoints.
std::optional<vve::Vec3> measure(vve::simple::Engine &engine, std::string_view state) {
    auto &renderer = engine.renderSystem().forward();
    const auto value = vve::test::linearRegion(renderer,56,62,16,8);
    const auto &p = renderer.targets.front().probes;
    if (value) std::println("state={} gi.probes.valid={} gi.probes.updated={} gi.near.gpu_bytes={} receiver.linear={:.7f},{:.7f},{:.7f}",
        state,p.valid,p.updated,p.gpuBytes,value->x,value->y,value->z);
    return value;
}
}
/// @brief The receiver's upward normal has zero cosine to the horizontal direct light illuminating the wall.
int main(int argc, char **argv) {
    using namespace vve;
    const bool offCamera = argc == 2 && std::string_view{argv[1]} == "--off-camera";
    const bool msaa = argc == 2 && std::string_view{argv[1]} == "--msaa4";
    if (argc > 1 && !offCamera && !msaa) return 3;
    auto engine = test::hiddenEngine("probe-bounce",PixelExtent{128,128});
    auto &render = engine.renderSystem();
    if (!engine.init() || !render.setRendererBackend(RendererBackend::hybrid_gi) || !render.setAnimationTime(AnimationTime{})) return 1;
    if (msaa && !render.setAntialiasing(Antialiasing::msaa4)) return 1;
    auto camera = Camera::lookAt(Position{.value = Vec3{0,1.5F,3}},Position{});
    camera.fov_y.radians = offCamera ? .45F : 1.1F;
    render.setCamera(camera);
    // Prove the entire contributor lies left of the main camera frustum in the off-camera variant.
    if (offCamera) {
        const auto clip = math::multiply(math::perspectiveVulkan(camera.fov_y.radians,1,.1F,100),
            math::lookAt(camera.position.value,Vec3{},Vec3{0,1,0}));
        for (auto corner : std::views::iota(0U,8U)) {
            const auto p = math::multiply(clip,Vec4{corner & 1 ? -2.0F : -2.2F,corner & 2 ? 3.0F : 0.0F,corner & 4 ? 2.0F : -2.0F,1});
            if (p.x >= -p.w) return 2;
        }
        std::println("main_view.wall_outside_frustum=1");
    }
    render.setBackgroundColor(LinearColor{.value = Vec3{0}});
    render.setDirectionalLight(Direction{.value = Vec3{-1,0,0}},LinearColor{},LightIntensity{4},LinearColor{.value = Vec3{0}});
    render.setPointLight(Position{},LinearColor{},LightIntensity{0},LightRange{1},LinearColor{.value = Vec3{0}});
    const auto receiver = render.addPlane(Vec2{1,1},LinearColor{.value = Vec3{.5F}});
    const auto wall = render.addCuboid(Vec3{-2.2F,0,-2},Vec3{-2,3,2},LinearColor{.value = Vec3{1,.01F,.01F}});
    if (!receiver || !wall) return 1;
    auto settings = render.giSettings(); settings.enabled = false;
    if (!render.setGiSettings(settings) || !warm(engine,2)) return 1;
    const auto off = measure(engine,"off");
    settings.enabled = true;
    if (!render.setGiSettings(settings) || !warm(engine)) return 1;
    const auto red = measure(engine,"red");
    const auto redPath = std::filesystem::path{VVE_TEST_TMP_DIR} / (offCamera ? "off-camera-red.png" : msaa ? "msaa-red.png" : "red.png");
    std::filesystem::create_directories(redPath.parent_path());
    if (!render.captureFrameToPng(redPath) || !warm(engine,32)) return 1;
    const auto later = measure(engine,"red-later");
    if (!render.setObjectColor(*wall,LinearColor{.value = Vec3{.01F,1,.01F}}) || !warm(engine)) return 1;
    const auto green = measure(engine,"green");
    if (!render.captureFrameToPng(redPath.parent_path() / (offCamera ? "off-camera-green.png" : msaa ? "msaa-green.png" : "green.png"))) return 1;
    if (!render.removeObject(*wall) || !warm(engine)) return 1;
    const auto removed = measure(engine,"removed");
    if (!off || !red || !later || !green || !removed) return 1;
    auto &renderer = render.forward();
    VkPhysicalDeviceProperties device{}; vkGetPhysicalDeviceProperties(renderer.physicalDevice.physicalDevice,&device);
    const auto delta = [](Vec3 a, Vec3 b) { return std::max({std::abs(a.x-b.x),std::abs(a.y-b.y),std::abs(a.z-b.z)}); };
    const float redRatio = red->x / std::max(red->y,.0001F), offRatio = off->x / std::max(off->y,.0001F);
    const float greenRatio = green->y / std::max(green->x,.0001F);
    const auto drift = delta(*red,*later), removal = delta(*removed,*off);
    std::println("device={} samples={} off_camera={} red_green_ratio={} off_ratio={} green_red_ratio={} convergence_delta={} removal_delta={} capture={} validation_active={}",
        std::string_view{device.deviceName},msaa ? 4 : 1,offCamera,redRatio,offRatio,greenRatio,drift,removal,redPath.string(),renderer.validationActive());
    // Saturation must change direction with the contributor; an ambient constant cannot satisfy this fixture.
    const bool pass = redRatio > offRatio + .5F && greenRatio > 1.5F && red->x > off->x + .005F &&
        green->y > off->y + .005F && drift < .002F && removal < .003F && renderer.targets.front().probes.valid == 64;
    render.waitIdle(); engine.gui().shutdownVulkan(); render.shutdown();
    std::println("validation.errors={}",renderer.validationErrorCount());
    return pass && renderer.validationErrorCount() == 0 ? 0 : 2;
}
