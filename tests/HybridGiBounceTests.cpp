/** @file @brief Real raster-probe color transport, off-camera capture, removal and feedback-free convergence. */
#include <vulkan/vulkan_core.h>
import std;
import VVEngine;
import VVEngine.Simple;
import VVEngine.Simple.Renderer;
import VVEngine.Simple.IrradianceProbes;
import VVEngine.Simple.Vulkan;
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
/// @brief Count presented frames, since a window acquisition retry does not refresh any probe.
bool completedFrames(vve::simple::Engine &engine, unsigned count) {
    const auto first=engine.renderSystem().renderedFrameCount();
    for (unsigned attempts=0;attempts<count*8+32 && engine.renderSystem().renderedFrameCount()-first<count;++attempts)
        if (!engine.renderFrame()) return false;
    return engine.renderSystem().renderedFrameCount()-first>=count;
}
/** @brief Static main lighting must be independent of pipeline cadence and contact-AO toggling.
 * Compare every linear HDR RGB channel in binary16 steps, before tonemapping and PNG encoding.
 * Ordinary batches keep normal frames in flight; explicit PNG captures exercise serialized draws.
 */
bool lightingCadence(vve::simple::Engine &engine, const std::filesystem::path &directory) {
    using namespace vve;
    auto &render=engine.renderSystem(); auto &renderer=render.forward();
    const auto original=render.giSettings();
    auto settings=original; settings.enabled=true; settings.ao_enabled=false;
    // Both volumes contain 64 probes and share a two-probe budget. Two complete sweeps
    // give this static scene a settled reference without averaging or relaxing its pixel gate.
    if (!render.setGiSettings(settings) || !completedFrames(engine,128)) return false;
    std::filesystem::create_directories(directory);
    simple::VulkanReadback readback;
    const auto &hdr=renderer.targets.front().hdrImage;
    if (readback.create(renderer.allocator,renderer.device.device,renderer.device.graphicsQueue,renderer.commandPool.commandPool,
        hdr.extent,VK_FORMAT_R16G16B16A16_SFLOAT)!=VK_SUCCESS) return false;
    using Pixels=std::vector<std::array<std::uint16_t,4>>;
    const auto capture=[&](std::string_view name) -> std::optional<Pixels> {
        if (readback.capture(hdr.image,0,VK_IMAGE_LAYOUT_GENERAL)!=VK_SUCCESS) return {};
        Pixels pixels(std::size_t(hdr.extent.width)*hdr.extent.height);
        if (readback.pixelBytes().size()!=pixels.size()*sizeof(pixels.front())) return {};
        std::memcpy(pixels.data(),readback.pixelBytes().data(),readback.pixelBytes().size());
        std::ofstream file{directory/(std::string{name}+".rgba16f"),std::ios::binary};
        file.write(reinterpret_cast<const char*>(pixels.data()),std::streamsize(readback.pixelBytes().size()));
        if (!file) return {};
        return pixels;
    };
    if (!render.captureFrameToPng(directory/"reference.png")) return false;
    const auto reference=capture("reference"); if (!reference) return false;
    const auto compare=[&](const Pixels &pixels,std::string_view state,bool monotone) {
        unsigned maximum=0,violations=0,invalid=0;
        for (std::size_t pixel=0;pixel<pixels.size();++pixel) for (unsigned channel=0;channel<3;++channel) {
            const auto a=(*reference)[pixel][channel],b=pixels[pixel][channel];
            const auto finite=[](unsigned value) { return (value&0x7c00U)!=0x7c00U && (!(value&0x8000U) || !(value&0x7fffU)); };
            if (!finite(a) || !finite(b)) { ++invalid; continue; }
            // Nonnegative IEEE binary16 bit patterns are their ordered representable-value indices.
            const int difference=int(b&0x7fffU)-int(a&0x7fffU);
            const unsigned steps=unsigned(monotone ? std::max(difference,0) : std::abs(difference));
            maximum=std::max(maximum,steps); violations+=steps>1;
        }
        std::println("lighting.cadence={} monotone={} half_step_max={} half_step_limit=1 violations={} invalid={}",
            state,monotone,maximum,violations,invalid);
        return violations==0 && invalid==0;
    };
    bool pass=true;
    for (const unsigned count : {1U,2U,3U,8U}) {
        if (!completedFrames(engine,count)) return false;
        const auto name=std::format("batch-{}",count);
        const auto pixels=capture(name);
        if (!pixels) return false;
        pass &= compare(*pixels,name,false);
    }
    for (unsigned frame=0;frame<4;++frame) {
        const auto name=std::format("serialized-{}",frame);
        if (!render.captureFrameToPng(directory/(name+".png"))) return false;
        const auto pixels=capture(name); if (!pixels) return false;
        pass &= compare(*pixels,name,false);
    }
    settings.ao_enabled=true;
    if (!render.setGiSettings(settings) || !completedFrames(engine,2)) return false;
    const auto ao=capture("ao-on"); if (!ao) return false;
    pass &= compare(*ao,"ao-on",true);
    settings.ao_enabled=false;
    if (!render.setGiSettings(settings) || !completedFrames(engine,2)) return false;
    const auto restored=capture("ao-restored"); if (!restored) return false;
    pass &= compare(*restored,"ao-restored",false);
    return render.setGiSettings(original).has_value() && completedFrames(engine,2) && pass;
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
    const bool cadence=lightingCadence(engine,redPath.parent_path()/(redPath.stem().string()+"-cadence"));
    if (!render.setObjectColor(*wall,LinearColor{.value = Vec3{.01F,1,.01F}}) || !warm(engine)) return 1;
    const auto green = measure(engine,"green");
    if (!render.captureFrameToPng(redPath.parent_path() / (offCamera ? "off-camera-green.png" : msaa ? "msaa-green.png" : "green.png"))) return 1;
    if (!render.removeObject(*wall) || !warm(engine)) return 1;
    const auto removed = measure(engine,"removed");
    if (!off || !red || !later || !green || !removed) return 1;
    // The transport receiver above has zero direct cosine. Exercise a visible sunlit
    // receiver and blocker too, so cadence changes in shared sun-shadow maps cannot hide.
    render.setCamera(Camera::lookAt(Position{.value=Vec3{0,1.5F,3}},Position{}, {},FovY{1.1F}));
    render.setDirectionalLight(Direction{.value=Vec3{-.7F,-1,-.3F}},LinearColor{},LightIntensity{3},LinearColor{.value=Vec3{0}});
    const auto blocker=render.addCuboid(Vec3{-.25F,0,-.25F},Vec3{.25F,.65F,.25F},LinearColor{.value=Vec3{.5F}});
    if (!blocker) return 1;
    const bool shadowCadence=lightingCadence(engine,redPath.parent_path()/(redPath.stem().string()+"-sun-shadow-cadence"));
    // Translating the same scene exposes cancellation in inverse-view-projection reconstruction.
    // Its camera-relative geometry, light direction and all comparison tolerances stay identical.
    const Transform translated{.translation=Position{.value=Vec3{91,6,130}}};
    if (!render.setObjectTransform(*receiver,translated) || !render.setObjectTransform(*blocker,translated)) return 1;
    render.setCamera(Camera::lookAt(Position{.value=Vec3{91,7.5F,133}},Position{.value=Vec3{91,6,130}}, {},FovY{1.1F}));
    const bool translatedCadence=lightingCadence(engine,redPath.parent_path()/(redPath.stem().string()+"-translated-shadow-cadence"));
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
        green->y > off->y + .005F && drift < .002F && removal < .003F && renderer.targets.front().probes.valid == 64 && cadence && shadowCadence && translatedCadence;
    render.waitIdle(); engine.gui().shutdownVulkan(); render.shutdown();
    std::println("validation.errors={}",renderer.validationErrorCount());
    return pass && renderer.validationErrorCount() == 0 ? 0 : 2;
}
