/** @file @brief Deterministic HDR contact, depth-edge, material-AO, resize and toggle receipts for spatial AO. */
#include <SDL3/SDL_video.h>
#include <vulkan/vulkan_core.h>
#include <vk_mem_alloc.h>
import std;
import VVEngine;
import VVEngine.Simple;
import VVEngine.Simple.Renderer;
import VVEngine.Simple.Scene;
import VVEngine.Simple.Vulkan;
import VVE.TestSupport;

namespace {
using namespace vve;
constexpr unsigned size = 256; ///< Small native HDR fixture; no exposure or tonemapping enters region comparisons.
auto directory = std::filesystem::path{VVE_TEST_TMP_DIR}; ///< Retained native fixtures, separated by registered subcommand.

/// @brief Keeps probes paused for AO isolation unless the active-probe subcommand explicitly warms them.
bool frames(simple::Engine &engine, unsigned count = 4) {
    for ([[maybe_unused]] auto i : std::views::iota(0U,count)) if (!engine.renderFrame()) return false;
    return true;
}
/// @brief Switches the existing public setting; exercising both fenced slots makes timestamps observable too.
bool ao(simple::Engine &engine, bool enabled) {
    auto settings = engine.renderSystem().giSettings(); settings.ao_enabled = enabled;
    if (!engine.renderSystem().setGiSettings(settings) || !frames(engine)) return false;
    const auto &renderer = engine.renderSystem().forward();
    const auto passes = renderer.lastRecordedPassOrder();
    using Pass = simple::ForwardRenderer::RecordedPass;
    const auto geometry = std::ranges::find(passes,Pass::gbuffer), lighting = std::ranges::find(passes,Pass::deferred_lighting);
    const auto contact = std::ranges::find(passes,Pass::contact_ao);
    return renderer.targets.front().aoEnabled == enabled &&
        (enabled ? geometry < contact && contact < lighting : contact == passes.end());
}
/// @brief Emits measured allocation and optional completed GPU timestamp receipts for this window.
void receipt(simple::ForwardRenderer &renderer) {
    const auto &target = renderer.targets.front();
    std::println("ao.enabled={} ao.gpu_bytes={} ao.gpu_ms={} gi.near.gpu_bytes={}", target.aoEnabled,
        target.aoGpuBytes, target.aoGpuMs ? std::format("{:.6f}", *target.aoGpuMs) : "unavailable", target.probes.gpuBytes);
}
/// @brief Projects a known world-space point into an explicit two-by-two linear HDR measurement region.
std::optional<Vec3> region(simple::Engine &engine, Vec3 point, std::string_view name) {
    auto &renderer = engine.renderSystem().forward();
    const auto &f = renderer.frameUniforms();
    const auto p = math::multiply(math::multiply(f.projection,f.view),Vec4{point.x,point.y,point.z,1});
    const int x = int((p.x / p.w * .5F + .5F) * f.viewport.x) - 1;
    const int y = int((p.y / p.w * .5F + .5F) * f.viewport.y) - 1;
    if (x < 0 || y < 0) return {};
    const auto value = test::linearRegion(renderer,x,y,2,2);
    if (value) std::println("region={} rect={},{},2,2 linear={:.7f},{:.7f},{:.7f}",name,x,y,value->x,value->y,value->z);
    return value;
}
/// @brief Maximum RGB difference prevents tint cancellation from hiding unexpected changes.
float delta(Vec3 a, Vec3 b) { return std::max({std::abs(a.x-b.x),std::abs(a.y-b.y),std::abs(a.z-b.z)}); }

/// @brief A binary glTF floor imports a genuine linear material AO texture without any renderer test override.
bool materialFloor(simple::Engine &engine) {
    const std::array<float,32> attributes{-10,0,-10, -10,0,10, 10,0,10, 10,0,-10,
        0,1,0, 0,1,0, 0,1,0, 0,1,0, 0,0, 0,1, 1,1, 1,0};
    const std::array<std::uint16_t,6> indices{0,1,2,0,2,3};
    std::filesystem::create_directories(directory);
    {
        std::ofstream binary{directory / "floor.bin",std::ios::binary};
        binary.write(reinterpret_cast<const char *>(attributes.data()),sizeof(attributes));
        binary.write(reinterpret_cast<const char *>(indices.data()),sizeof(indices));
    }
    (void)test::writeFixture(directory,"ao.ppm",std::string{"P6\n1 1\n255\n"}+std::string(3,char(64)));
    const auto file = test::writeFixture(directory,"floor.gltf",R"({"asset":{"version":"2.0"},
      "buffers":[{"uri":"floor.bin","byteLength":140}],
      "bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":48},{"buffer":0,"byteOffset":48,"byteLength":48},
        {"buffer":0,"byteOffset":96,"byteLength":32},{"buffer":0,"byteOffset":128,"byteLength":12}],
      "accessors":[{"bufferView":0,"componentType":5126,"count":4,"type":"VEC3","min":[-10,0,-10],"max":[10,0,10]},
        {"bufferView":1,"componentType":5126,"count":4,"type":"VEC3"},{"bufferView":2,"componentType":5126,"count":4,"type":"VEC2"},
        {"bufferView":3,"componentType":5123,"count":6,"type":"SCALAR"}],
      "images":[{"uri":"ao.ppm"}],"textures":[{"source":0}],
      "materials":[{"pbrMetallicRoughness":{"baseColorFactor":[0.5,0.5,0.5,1],"metallicFactor":0,"roughnessFactor":1},
        "occlusionTexture":{"index":0}}],
      "meshes":[{"primitives":[{"attributes":{"POSITION":0,"NORMAL":1,"TEXCOORD_0":2},"indices":3,"material":0}]}],
      "nodes":[{"mesh":0}],"scenes":[{"nodes":[0]}],"scene":0})");
    const auto scene = engine.assets().loadScene(file);
    if (!scene || !engine.renderSystem().instantiateScene(*scene)) return false;
    return std::ranges::any_of(engine.renderSystem().renderMaterials(),[](const auto &m) {
        return m.ambient_occlusion_texture_index != simple::kNoTexture;
    });
}
/// @brief Explicit ambient radiance plus a sun isolates AO from direct lighting and keeps all comparisons deterministic.
void sun(simple::RenderSystem &render, float intensity) {
    render.setDirectionalLight(Direction{.value = Vec3{-.4F,-1,-.25F}},LinearColor{},LightIntensity{intensity},LinearColor{.value = Vec3{0}});
}
/// @brief Records contact, open-floor, unlit/emissive and sky regions with identical camera, time and lighting.
bool contact(simple::Engine &engine, bool material, bool probes) {
    auto &render = engine.renderSystem(); auto &renderer = render.forward();
    render.setCamera(Camera::lookAt(Position{.value = Vec3{2.5F,2.8F,4}},Position{.value = Vec3{0,.1F,0}}));
    if (material ? !materialFloor(engine) : !render.addPlane(Vec2{10,10},LinearColor{.value = Vec3{.5F}})) return false;
    if (!render.addCuboid(Vec3{-.5F,0,-.5F},Vec3{.5F,1,.5F},LinearColor{.value = Vec3{.5F}})) return false;
    const auto unlit = render.addCuboid(Vec3{-.2F},Vec3{.2F},LinearColor{.value = Vec3{.3F,.7F,.1F}},
        Transform{.translation = Position{.value = Vec3{-1.5F,.5F,.6F}}});
    if (!unlit || !render.setObjectUnlit(*unlit,true)) return false;
    (void)test::writeFixture(directory,"emitter.mtl","newmtl glow\nKd 0 0 0\nKe 0.3 1.2 0.2\n");
    const auto emitter = engine.assets().loadScene(test::writeFixture(directory,"emitter.obj",
        "mtllib emitter.mtl\nusemtl glow\nv 1.1 0.3 0\nv 1.7 0.3 0\nv 1.7 1.0 0\nv 1.1 1.0 0\nvn 0 0 1\nf 1//1 2//1 3//1\nf 1//1 3//1 4//1\n"));
    if (!emitter || !render.instantiateScene(*emitter)) return false;
    // All points are on known surfaces; the contact patch lies 6 cm outside the front cube edge.
    const std::array points{Vec3{0,0,.56F},Vec3{2,0,1},Vec3{-1.5F,.5F,.81F},Vec3{1.4F,.65F,0}};
    const std::array names{"contact","open-sun","unlit","emissive"};
    const auto measure = [&](std::string_view state) -> std::optional<std::array<Vec3,5>> {
        std::array<Vec3,5> values{};
        for (auto i : std::views::iota(0U,4U)) {
            const auto value = region(engine,points[i],std::format("{}-{}",state,names[i]));
            if (!value) return {}; values[i] = *value;
        }
        const auto sky = test::linearRegion(renderer,126,4,4,4);
        if (!sky) return {}; values[4] = *sky;
        std::println("region={}-sky linear={},{},{}",state,sky->x,sky->y,sky->z); receipt(renderer);
        if (!render.captureFrameToPng(directory / std::format("{}-{}.png",material ? "material" : probes ? "probes" : "contact",state))) return {};
        return values;
    };
    if (probes) {
        auto settings = render.giSettings(); settings.enabled = true;
        if (!render.setGiSettings(settings) || !frames(engine,64)) return false;
    }
    sun(render,0.8F);
    if (probes && !frames(engine,64)) return false;
    if (!ao(engine,false)) return false;
    const auto off = measure("off");
    if (!ao(engine,true)) return false;
    const auto on = measure("on");
    if (!off || !on) return false;
    const auto fraction = ((*off)[0].x-(*on)[0].x) / std::max((*off)[0].x,.000001F);
    const float openDelta = delta((*off)[1],(*on)[1]);
    const float unlitDelta = delta((*off)[2],(*on)[2]), emissionDelta = delta((*off)[3],(*on)[3]), skyDelta = delta((*off)[4],(*on)[4]);
    std::println("contact.total_fraction={} open.sun.delta={} unlit.delta={} emissive.delta={} sky.delta={}",fraction,openDelta,unlitDelta,emissionDelta,skyDelta);
    bool pass = openDelta < .002F && unlitDelta < .0005F && emissionDelta < .0005F && skyDelta < .0005F;
    // With direct light removed, the off region is entirely indirect; a 10% gate distinguishes contact from FP16 noise.
    sun(render,0);
    if (probes && !frames(engine,64)) return false;
    if (!ao(engine,false)) return false;
    const auto ambientOff = measure("indirect-off");
    if (!ao(engine,true)) return false;
    const auto ambientOn = measure("indirect-on");
    if (!ambientOff || !ambientOn) return false;
    const float indirectFraction = ((*ambientOff)[0].x-(*ambientOn)[0].x) / std::max((*ambientOff)[0].x,.000001F);
    const float openAmbient = delta((*ambientOff)[1],(*ambientOn)[1]);
    std::println("contact.indirect.off={} contact.indirect.on={} contact.indirect.fraction={} contact.limit=0.10 open.indirect.delta={}",
        (*ambientOff)[0].x,(*ambientOn)[0].x,indirectFraction,openAmbient);
    if (material) {
        // Imported AO=64/255 is stronger than this contact SSAO: min must leave it unchanged, rather than stacking occlusion.
        const float error = delta((*ambientOff)[0],(*ambientOn)[0]);
        std::println("material.ao={} material.min_prediction={} material.actual={} material.min_error={} material.limit=0.0005",
            64.0F/255,(*ambientOff)[0].x,(*ambientOn)[0].x,error);
        pass &= error < .0005F && (*ambientOff)[0].x > .02F && (*ambientOff)[0].x < .05F;
    } else { pass &= indirectFraction > .10F; }
    if (!probes) {
        const float directDelta = std::abs(((*off)[0].x-(*ambientOff)[0].x)-((*on)[0].x-(*ambientOn)[0].x));
        std::println("contact.direct.delta={} direct.limit=0.002",directDelta); pass &= directDelta < .002F;
    }
    return pass && openAmbient < .002F;
}

/// @brief Reads all linear HDR pixels only for bounded diagnostic equality and silhouette maximum-error checks.
std::optional<std::vector<std::uint16_t>> hdr(simple::ForwardRenderer &renderer) {
    const auto &image = renderer.targets.front().hdrImage;
    simple::VulkanReadback readback;
    if (readback.create(renderer.allocator,renderer.device.device,renderer.device.graphicsQueue,renderer.commandPool.commandPool,
        image.extent,image.format) != VK_SUCCESS || readback.capture(image.image,0,VK_IMAGE_LAYOUT_GENERAL) != VK_SUCCESS) return {};
    std::vector<std::uint16_t> pixels(readback.pixelBytes().size()/2);
    std::memcpy(pixels.data(),readback.pixelBytes().data(),readback.pixelBytes().size()); return pixels;
}
/// @brief A floating foreground cube and distant wall have no local occluder: every silhouette sample must stay unchanged.
bool halo(simple::Engine &engine) {
    auto &render = engine.renderSystem(); auto &renderer = render.forward();
    render.setCamera(Camera::lookAt(Position{.value = Vec3{0,1,5}},Position{.value = Vec3{0,1,0}}));
    if (!render.addCuboid(Vec3{-.5F,.5F,-.5F},Vec3{.5F,1.5F,.5F},LinearColor{.value = Vec3{.5F}}) ||
        !render.addCuboid(Vec3{-10,-10,-6.1F},Vec3{10,10,-6},LinearColor{.value = Vec3{.35F}}) || !ao(engine,false)) return false;
    const auto off = hdr(renderer);
    const auto wallOff = test::linearRegion(renderer,155,120,2,12), cubeOff = test::linearRegion(renderer,124,124,8,8);
    if (!render.captureFrameToPng(directory / "halo-off.png") || !ao(engine,true)) return false;
    const auto on = hdr(renderer);
    const auto wallOn = test::linearRegion(renderer,155,120,2,12), cubeOn = test::linearRegion(renderer,124,124,8,8);
    if (!off || !on || !wallOff || !wallOn || !cubeOff || !cubeOn || off->size() != on->size()) return false;
    float maximum{};
    for (auto pixel : std::views::iota(0U,size*size)) for (auto channel : {0U,1U,2U})
        maximum = std::max(maximum,std::abs(test::linearHalf((*off)[4*pixel+channel])-test::linearHalf((*on)[4*pixel+channel])));
    std::println("halo.wall.off={} halo.wall.on={} halo.cube.off={} halo.cube.on={} halo.edge_max_delta={} halo.limit=0.002",
        wallOff->x,wallOn->x,cubeOff->x,cubeOn->x,maximum); receipt(renderer);
    return render.captureFrameToPng(directory / "halo-on.png").has_value() && maximum < .002F &&
        delta(*wallOff,*wallOn) < .002F && delta(*cubeOff,*cubeOn) < .002F && cubeOff->x - wallOff->x > .03F;
}
/// @brief A toggle publishes no stale image when disabled, and the HDR result returns bit-for-bit to the first off frame.
bool toggle(simple::Engine &engine) {
    auto &render = engine.renderSystem(); auto &renderer = render.forward();
    if (!render.addPlane(Vec2{10,10},LinearColor{.value = Vec3{.5F}}) ||
        !render.addCuboid(Vec3{-.5F,0,-.5F},Vec3{.5F,1,.5F},LinearColor{.value = Vec3{.5F}}) || !ao(engine,false)) return false;
    const auto before = hdr(renderer); const auto idle = renderer.deviceWaitIdleCount();
    if (!ao(engine,true)) return false;
    const auto on = hdr(renderer); receipt(renderer);
    if (!ao(engine,false)) return false;
    const auto after = hdr(renderer); receipt(renderer);
    using Pass = simple::ForwardRenderer::RecordedPass;
    const auto passes = renderer.lastRecordedPassOrder();
    const bool same = before && on && after && *before == *after;
    const bool changed = before && on && *before != *on;
    const bool skipped = std::ranges::count(passes,Pass::contact_ao) == 0;
    std::println("toggle.bit_identical={} toggle.on_changed={} toggle.off_pass_skipped={} toggle.idle_delta={}",same,changed,skipped,renderer.deviceWaitIdleCount()-idle);
    return same && changed && skipped && renderer.deviceWaitIdleCount() == idle;
}
/// @brief Resizing recreates both screen images at the existing idle boundary while retaining the fixed-size probes.
bool resize(simple::Engine &engine) {
    auto &renderer = engine.renderSystem().forward();
    if (!ao(engine,true)) return false;
    auto &target = renderer.targets.front();
    const auto bytes = target.aoGpuBytes, probes = target.probes.gpuBytes;
    const auto extent = target.aoImages[0].extent;
    std::println("resize.before.extent={}x{}",extent.width,extent.height); receipt(renderer);
    if (!SDL_SetWindowSize(target.window,128,128) || !SDL_SyncWindow(target.window) || !frames(engine)) return false;
    const auto next = target.aoImages[0].extent;
    std::uint64_t measured{};
    for (const auto &image : target.aoImages) {
        VmaAllocationInfo info{}; vmaGetAllocationInfo(renderer.allocator,image.allocation,&info); measured += info.size;
        if (image.extent.width != next.width || image.extent.height != next.height || image.samples != VK_SAMPLE_COUNT_1_BIT) return false;
    }
    std::println("resize.after.extent={}x{} resize.vma_bytes={} resize.payload_bytes={}",next.width,next.height,measured,4*next.width*next.height); receipt(renderer);
    return next.width == 128 && next.height == 128 && target.aoGpuBytes == measured && measured < bytes &&
        measured >= 4*next.width*next.height && target.probes.gpuBytes == probes && probes == 319232 && target.aoEnabled;
}
} // namespace

/// @brief Registered subcommands exercise the real Vulkan implementation with validation through teardown.
int main(int argc, char **argv) {
    using namespace vve;
    const std::string_view mode = argc == 2 ? argv[1] : "contact";
    const std::array modes{"contact","msaa4","halo","halo-msaa4","material","resize","toggle","probes"};
    if (argc > 2 || std::ranges::find(modes,mode) == modes.end()) return 3;
    directory /= mode;
    std::filesystem::create_directories(directory);
    auto engine = test::hiddenEngine("hybrid-gi-ao",PixelExtent{size,size}); auto &render = engine.renderSystem();
    if (!engine.init() || !render.setRendererBackend(RendererBackend::hybrid_gi) ||
        !render.setAntialiasing(mode == "msaa4" || mode == "halo-msaa4" ? Antialiasing::msaa4 : Antialiasing::none) ||
        !render.setGiSettings(GiSettings{.enabled = false}) || !render.setAnimationTime(AnimationTime{})) return 1;
    render.setBackgroundColor(LinearColor{.value = Vec3{.2F,.3F,.4F}});
    const auto sky = test::writeFixture(directory,"sky.ppm",std::string{"P6\n2 1\n255\n"}+std::string{"\x50\x80\xa0\x50\x80\xa0",6});
    if (!render.setSkybox(sky)) return 1;
    render.setPointLight(Position{},LinearColor{},LightIntensity{0},LightRange{1},LinearColor{.value = Vec3{.3F}}); render.clearLights();
    bool pass = mode == "halo" || mode == "halo-msaa4" ? halo(engine) : mode == "toggle" ? toggle(engine) : mode == "resize" ? resize(engine)
        : contact(engine,mode == "material",mode == "probes");
    auto &renderer = render.forward();
    VkPhysicalDeviceProperties device{}; vkGetPhysicalDeviceProperties(renderer.physicalDevice.physicalDevice,&device);
    const bool validation = renderer.validationActive();
    std::println("device={} device.type={} samples={} validation_active={} mode={}",std::string_view{device.deviceName},int(device.deviceType),unsigned(renderer.sampleCount()),validation,mode);
    receipt(renderer);
    render.waitIdle(); engine.gui().shutdownVulkan(); render.shutdown();
    std::println("validation.errors={} test.pass={}",renderer.validationErrorCount(),pass);
#ifndef NDEBUG
    pass &= validation;
#endif
    return pass && renderer.validationErrorCount() == 0 ? 0 : 2;
}
