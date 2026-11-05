/** @file @brief Linear HDR sky energy and emissive transport through real raster-probe captures. */
#include <vulkan/vulkan_core.h>
import std;
import VVEngine;
import VVEngine.Simple;
import VVEngine.Simple.Renderer;
import VVEngine.Simple.IrradianceProbes;
import VVEngine.Simple.Vulkan;
import VVE.TestSupport;

namespace {
/// @brief Replaces every probe twice so both fenced slots observe a converged scene.
bool warm(vve::simple::Engine &engine, unsigned frames = 2 * vve::simple::IrradianceProbes::count / vve::simple::IrradianceProbes::maxBudget) {
    for (auto frame : std::views::iota(0U, frames)) { if (!engine.renderFrame()) return false; }
    return true;
}
/// @brief Reads only the grey receiver, before exposure, tonemapping and display encoding.
std::optional<vve::Vec3> measure(vve::simple::Engine &engine, std::string_view state) {
    auto &renderer = engine.renderSystem().forward();
    const auto value = vve::test::linearRegion(renderer, 56, 62, 16, 8);
    if (value) std::println("state={} receiver.linear={:.7f},{:.7f},{:.7f} gi.probes.valid={}",
        state, value->x, value->y, value->z, renderer.targets.front().probes.valid);
    return value;
}
/// @brief Maximum channel error keeps a tinted contribution from hiding in a luminance average.
float difference(vve::Vec3 a, vve::Vec3 b) {
    return std::max({std::abs(a.x-b.x), std::abs(a.y-b.y), std::abs(a.z-b.z)});
}
/// @brief Checks per-channel energy and hue against the existing environment response.
bool skyEnergy(vve::Vec3 off, vve::Vec3 on, std::string_view name) {
    const vve::Vec3 relative{std::abs(on.x-off.x) / off.x, std::abs(on.y-off.y) / off.y, std::abs(on.z-off.z) / off.z};
    const float error = std::max({relative.x, relative.y, relative.z});
    std::println("sky={} relative_error={} limit=0.25 blue_dominant.off={} blue_dominant.on={}",
        name, error, off.z > off.y && off.y > off.x, on.z > on.y && on.y > on.x);
    // Constant radiance is exactly L0. A horizon step's cosine convolution is 1.25L at L1 (true L),
    // bounding SH truncation at 25% for this open fixture; small Fresnel/filter/FP16 differences fit inside it.
    return off.x > .01F && error < .25F && off.z > off.y && off.y > off.x && on.z > on.y && on.y > on.x;
}
/** @brief The same physical probe must capture identical cloudy sky in either temporary atlas slot.
 * Control only scheduler ages, then use the actual renderer's capture shader and atlas. No CPU copy
 * of sky/cloud math is involved. The regression originally failed because ray-march jitter hashed
 * absolute atlas pixels, changing radiance whenever a probe moved between the two capture slots.
 */
bool skyCaptureSlots(vve::simple::Engine &engine, const std::filesystem::path &directory) {
    using namespace vve;
    using Probes=simple::IrradianceProbes;
    static_assert(Probes::maxBudget==2);
    auto &render=engine.renderSystem(); auto &renderer=render.forward();
    auto settings=render.giSettings(); settings.enabled=true; settings.ao_enabled=false; settings.quality=GiQuality::medium;
    if (!render.setGiSettings(settings) || !render.setSky(SkySettings{.enabled=true,.sun_size=0,.cloud_coverage=.35F})
        || !warm(engine,2)) return false;
    auto &target=renderer.targets.front();
    simple::VulkanReadback readback;
    if (readback.create(renderer.allocator,renderer.device.device,renderer.device.graphicsQueue,renderer.commandPool.commandPool,
        target.probes.coefficients.extent,VK_FORMAT_R32G32B32A32_SFLOAT)!=VK_SUCCESS) return false;
    bool good=true; float maximumCoefficient=0;
    for (unsigned cascade=0;cascade<2;++cascade) {
        auto &probes=cascade==0 ? target.probes : target.coarseProbes;
        std::array<std::array<float,4>,4> reference{};
        unsigned violations=0,comparisons=0;
        float maximumError=0;
        constexpr float tolerance=8*std::numeric_limits<float>::epsilon();
        const auto selectSlot=[&](unsigned slot) {
            for (auto *volume : {&target.probes,&target.coarseProbes}) {
                volume->captured.fill(100); volume->generation.fill(std::numeric_limits<std::uint64_t>::max());
            }
            target.probes.clock=200;
            probes.captured[0]=slot+1; probes.captured[1]=2-slot;
        };
        // Read the transfer-enabled coefficient image: four RGB SH terms and explicit validity.
        // Probe zero's fixed toroidal row stays zero regardless of its temporary capture slot.
        const auto compareCoefficients=[&](bool establishReference) {
            if (readback.capture(probes.coefficients.image,0,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)!=VK_SUCCESS) return false;
            std::array<std::array<float,4>,4> values{};
            std::memcpy(values.data(),readback.pixelBytes().data(),sizeof(values));
            for (unsigned coefficient=0;coefficient<4;++coefficient) {
                good &= values[coefficient][3]==1;
                for (unsigned channel=0;channel<3;++channel) {
                    const float value=values[coefficient][channel];
                    good &= std::isfinite(value);
                    maximumCoefficient=std::max(maximumCoefficient,std::abs(value));
                    if (!establishReference) {
                        const float expected=reference[coefficient][channel];
                        const float error=std::abs(value-expected)/std::max({1.0F,std::abs(value),std::abs(expected)});
                        maximumError=std::max(maximumError,error); violations+=!(error<=tolerance); ++comparisons;
                    }
                }
                if (!establishReference) good &= values[coefficient][3]==reference[coefficient][3];
            }
            if (establishReference) reference=values;
            return true;
        };
        // A-B-A exercises relocation and return. Row zero denotes one unchanged world-space probe.
        for (unsigned phase=0;phase<3;++phase) {
            const unsigned slot=phase==1 ? 1U : 0U;
            selectSlot(slot);
            const auto name=std::format("sky-slot-{}-{}",cascade,phase);
            // This explicit capture waits for a rendered frame; a skipped window step cannot pass.
            if (!render.captureFrameToPng(directory/(name+".png")) || probes.updated!=2 || probes.selected[slot]!=0
                || !compareCoefficients(phase==0)) return false;
        }
        std::println("sky.capture_slot_cascade={} comparisons={} normalized_float_error={} limit={} violations={}",
            cascade,comparisons,maximumError,tolerance,violations);
        good &= comparisons==24 && violations==0;
        // Two consecutive batches end on opposite slots. No readback/idle is inserted between
        // submissions: each frame's usual fence protects uniforms while prior GPU work may run.
        for (unsigned batch=0;batch<2;++batch) {
            for (unsigned frame=0;frame<8;++frame) {
                const unsigned slot=(frame+batch)%2;
                const auto before=render.renderedFrameCount();
                for (unsigned attempt=0;attempt<8 && render.renderedFrameCount()==before;++attempt) {
                    selectSlot(slot);
                    if (!engine.renderFrame()) return false;
                }
                if (render.renderedFrameCount()==before || probes.updated!=2 || probes.selected[slot]!=0) return false;
            }
            if (!compareCoefficients(false)) return false;
        }
        std::println("sky.capture_pipeline_cascade={} comparisons={} normalized_float_error={} limit={} violations={}",
            cascade,comparisons,maximumError,tolerance,violations);
        good &= comparisons==48 && violations==0;
    }
    std::println("sky.capture_slot_max_coefficient={} minimum=0.01",maximumCoefficient);
    return good && maximumCoefficient>.01F;
}
/// @brief Independent L1 cosine integral from six ordinary main-view sky renders, without copying sky shader math.
std::optional<vve::Vec3> physicalSkyReference(vve::simple::Engine &engine) {
    using namespace vve;
    auto &render = engine.renderSystem();
    auto &renderer = render.forward();
    constexpr std::array directions{Vec3{1,0,0}, Vec3{-1,0,0}, Vec3{0,1,0}, Vec3{0,-1,0}, Vec3{0,0,1}, Vec3{0,0,-1}};
    constexpr auto size = simple::IrradianceProbes::faceSize;
    static_assert(size <= 128 && 128 % size == 0 && (128 / size) % 2 == 0);
    simple::VulkanReadback readback;
    if (readback.create(renderer.allocator, renderer.device.device, renderer.device.graphicsQueue, renderer.commandPool.commandPool,
        {128,128}, VK_FORMAT_R16G16B16A16_SFLOAT) != VK_SUCCESS) return {};
    Vec3 integral{};
    float weights{};
    // Six bounded readbacks sample the exact shared sky shader at the probe origin; no surface, fog or tone curve enters.
    for (auto face : std::views::iota(0U, static_cast<unsigned>(directions.size()))) {
        render.setCamera(Camera::lookAt(Position{}, Position{.value = directions[face]}, {}, FovY{std::numbers::pi_v<float> / 2}));
        if (!engine.renderFrame() || readback.capture(renderer.targets.front().hdrImage.image, 0, VK_IMAGE_LAYOUT_GENERAL) != VK_SUCCESS) return {};
        for (auto y : std::views::iota(0U, size)) {
            for (auto x : std::views::iota(0U, size)) {
                Vec3 radiance{};
                // Average the four samples around each 16-face texel centre in the higher-resolution main view.
                for (auto dy : std::views::iota(0U, 2U)) {
                    for (auto dx : std::views::iota(0U, 2U)) {
                        const auto column = x * (128 / size) + (128 / size) / 2 - 1 + dx;
                        const auto row = y * (128 / size) + (128 / size) / 2 - 1 + dy;
                        std::array<std::uint16_t,4> texel{};
                        std::memcpy(texel.data(), readback.pixelBytes().data() + (row * 128 + column) * 8, 8);
                        radiance = math::add(radiance, Vec3{test::linearHalf(texel[0]), test::linearHalf(texel[1]), test::linearHalf(texel[2])});
                    }
                }
                const float u = (x + .5F) / size * 2 - 1, v = (y + .5F) / size * 2 - 1;
                const float inverseLength = 1 / std::sqrt(1 + u*u + v*v);
                const float directionY = (face == 2 ? 1 : face == 3 ? -1 : -v) * inverseLength;
                const float weight = inverseLength * inverseLength * inverseLength;
                // Y00²=1/(4pi), (2/3)Y1²=1/(2pi): normalized L1 E/pi kernel is (1 + 2*n.d).
                integral = math::add(integral, math::scale(radiance, .25F * weight * (1 + 2 * directionY)));
                weights += weight;
            }
        }
    }
    return math::scale(integral, 1 / weights);
}
}

/// @brief Isolates sky or emission, with black background, no direct lights and paused animation.
int main(int argc, char **argv) {
    using namespace vve;
    const bool emissive = argc == 2 && std::string_view{argv[1]} == "--emissive";
    if (argc > 1 && !emissive) return 3;
    const auto directory = std::filesystem::path{VVE_TEST_TMP_DIR} / (emissive ? "emissive" : "sky");
    auto engine = test::hiddenEngine("probe-sky-emission", PixelExtent{128,128});
    auto &render = engine.renderSystem();
    if (!engine.init() || !render.setRendererBackend(RendererBackend::hybrid_gi) ||
        !render.setAnimationTime(AnimationTime{}) || !render.setMaterialLighting(MaterialLighting::physically_based)) return 1;
    // The first point light's ambient controls the scene-wide constant, which clearLights deliberately retains.
    render.setPointLight(Position{}, LinearColor{}, LightIntensity{0}, LightRange{1}, LinearColor{.value = Vec3{0}});
    render.clearLights();
    render.setBackgroundColor(LinearColor{.value = Vec3{0}});
    // Put the receiver inside the unfaded grid interior (x=-2..0, z=0..2 for this camera), not its fallback border.
    const Position centre{.value = Vec3{-1,0,1}};
    render.setCamera(Camera::lookAt(Position{.value = Vec3{0,1.5F,3}}, centre, {}, FovY{1.1F}));
    const auto receiver = render.addPlane(Vec2{1,1}, LinearColor{.value = Vec3{.5F}}, Transform{.translation = centre});
    if (!receiver || !render.setObjectSurface(*receiver, SurfaceMaterial{.roughness = 1})) return 1;
    std::optional<RenderObjectHandle> panel;
    if (emissive) {
        // Imported Ke supplies radiance independently of direct lighting; the vertical panel is beside the receiver.
        (void)test::writeFixture(directory, "panel.mtl", "newmtl glow\nKd 0 0 0\nKe 8 0.08 0.08\n");
        const auto path = test::writeFixture(directory, "panel.obj", "mtllib panel.mtl\no panel\nusemtl glow\n"
            "v -2 0 -2\nv -2 3 -2\nv -2 3 2\nv -2 0 2\nvn 1 0 0\nf 1//1 2//1 3//1\nf 1//1 3//1 4//1\n");
        const auto scene = engine.assets().loadScene(path);
        if (!scene) return 1;
        const auto instance = render.instantiateScene(*scene);
        if (!instance) return 1;
        const auto objects = render.sceneInstanceObjects(*instance);
        if (!objects || objects->size() != 1 || !std::ranges::any_of(render.renderMaterials(),
            [](const auto &material) { return material.emissive.value.x > 7; })) return 1;
        panel = objects->front();
    } else {
        // Uniform sRGB panorama: blue > green > red. A black clear colour makes missing sky immediately visible.
        const auto path = test::writeFixture(directory, "sky.ppm", std::string{"P6\n2 1\n255\n"} +
            std::string{"\x70\xa6\xee\x70\xa6\xee", 6});
        if (!render.setSkybox(path)) return 1;
    }
    auto settings = render.giSettings(); settings.enabled = false;
    if (!render.setGiSettings(settings) || !warm(engine, 2)) return 1;
    const auto off = measure(engine, "off");
    if (!render.captureFrameToPng(directory / "off.png")) return 1;
    settings.enabled = true;
    if (!render.setGiSettings(settings) || !warm(engine)) return 1;
    const auto on = measure(engine, "on");
    if (!render.captureFrameToPng(directory / "on.png") || !off || !on) return 1;
    bool pass = render.forward().targets.front().probes.valid == simple::IrradianceProbes::count;
    if (emissive) {
        // setObjectColor replaces the imported material with a black, non-emissive material, preserving geometry.
        if (!render.setObjectColor(*panel, LinearColor{.value = Vec3{0}}) || !warm(engine)) return 1;
        const auto zero = measure(engine, "emission-zero");
        if (!zero || !render.captureFrameToPng(directory / "emission-zero.png")) return 1;
        const auto delta = difference(*zero, *off);
        std::println("emissive.red_shift={} emissive.red_green_ratio={} emission_zero.delta={} limit=0.003",
            on->x-off->x, on->x / std::max(on->y, .0001F), delta);
        // Same 0.003 return tolerance as the bounce fixture; strong red Ke must transport >0.005 linear units.
        pass &= on->x > off->x + .005F && on->x > 1.5F * on->y && delta < .003F;
    } else {
        pass &= skyEnergy(*off, *on, "panorama");
        // Authored global and per-light ambient must be replaced at full probe coverage, rather than stacked on GI.
        render.setPointLight(Position{}, LinearColor{}, LightIntensity{0}, LightRange{1}, LinearColor{.value = Vec3{.6F}});
        render.addDirectionalLight(Direction{}, LinearColor{}, LightIntensity{0}, LinearColor{.value = Vec3{.4F}});
        if (!warm(engine)) return 1;
        const auto ambient = measure(engine, "authored-ambient");
        if (!ambient) return 1;
        const auto ambientDelta = difference(*ambient, *on);
        std::println("sky.authored_ambient_delta={} limit=0.002", ambientDelta);
        pass &= ambientDelta < .002F;
        render.setPointLight(Position{}, LinearColor{}, LightIntensity{0}, LightRange{1}, LinearColor{.value = Vec3{0}});
        render.clearLights();
        // The physical sky uses the same pipeline and device capabilities; no directional lights means no sun disk.
        render.clearSkybox();
        if (!render.setSky(SkySettings{.enabled = true, .sun_size = 0})) return 1;
        settings.enabled = false;
        if (!render.setGiSettings(settings) || !warm(engine, 2)) return 1;
        const auto physicalOff = measure(engine, "physical-off");
        if (!render.captureFrameToPng(directory / "physical-off.png")) return 1;
        settings.enabled = true;
        if (!render.setGiSettings(settings) || !warm(engine)) return 1;
        const auto physicalOn = measure(engine, "physical-on");
        if (!physicalOff || !physicalOn || !render.captureFrameToPng(directory / "physical-on.png")) return 1;
        // Capture a complete sweep while the camera has dense haze. Remove haze for one main-view measurement;
        // only one batch is refreshed, so wrongly fogged sky radiance would remain in almost every probe.
        if (!render.setAtmosphere(AtmosphereSettings{.density = .02F}) || !warm(engine) ||
            !render.setAtmosphere(AtmosphereSettings{}) || !warm(engine, 1)) return 1;
        const auto afterHaze = measure(engine, "physical-after-capture-haze");
        if (!afterHaze) return 1;
        const auto delta = difference(*afterHaze, *physicalOn);
        std::println("sky.capture_haze_delta={} limit=0.002", delta);
        pass &= delta < .002F; // Existing bounce stability tolerance, with identical physical sky and no animation.
        // The legacy physical-sky environment is a six-step zenith/horizon approximation, unlike skyColour's
        // directional 16-step integral. Compare GI against the actual rendered sky, while printing both paths above.
        settings.enabled = false;
        if (!render.setGiSettings(settings) || !render.setObjectColor(*receiver, LinearColor{.value = Vec3{0}}) ||
            !render.setObjectSurface(*receiver, SurfaceMaterial{.roughness = 1}) || !warm(engine, 2)) return 1;
        const auto black = measure(engine, "physical-specular-air");
        if (!black || !render.setObjectVisible(*receiver, false)) return 1;
        const auto reference = physicalSkyReference(engine);
        if (!reference) return 1;
        const auto irradiance = math::scale(math::subtract(*physicalOn, *black), 2.0F); // Remove unchanged specular/air and divide by grey albedo.
        const float relative = std::max({std::abs(irradiance.x-reference->x) / reference->x,
            std::abs(irradiance.y-reference->y) / reference->y, std::abs(irradiance.z-reference->z) / reference->z});
        std::println("sky=physical reference.e_over_pi={},{},{} receiver.e_over_pi={},{},{} relative_error={} limit=0.03",
            reference->x, reference->y, reference->z, irradiance.x, irradiance.y, irradiance.z, relative);
        // Identical 16-face L1 quadrature: 3% covers FP16, subpixel sky filtering and probe/main-view air/height differences.
        pass &= relative < .03F && irradiance.x > .01F && irradiance.z > irradiance.y && irradiance.y > irradiance.x;
        pass &= skyCaptureSlots(engine,directory);
    }
    auto &renderer = render.forward();
    VkPhysicalDeviceProperties device{}; vkGetPhysicalDeviceProperties(renderer.physicalDevice.physicalDevice, &device);
    const bool validation = renderer.validationActive();
    render.waitIdle(); engine.gui().shutdownVulkan(); render.shutdown();
    std::println("device={} validation_active={} validation.errors={} captures={}",
        std::string_view{device.deviceName}, validation, renderer.validationErrorCount(), directory.string());
#ifndef NDEBUG
    pass &= validation; // Debug acceptance requires the installed validation layer; Release has no messenger.
#endif
    return pass && renderer.validationErrorCount() == 0 ? 0 : 2;
}
