/** @file @brief Capture-sun coverage, coarse color transport, cascade continuity, scrolling and measured memory/budget receipts. */
#include <vulkan/vulkan_core.h>
#include "../src/versions/simple/shaders/simple_shared.h"
import std;
import VVEngine;
import VVEngine.Simple;
import VVEngine.Simple.Renderer;
import VVEngine.Simple.IrradianceProbes;
import VVEngine.Simple.Vulkan;
import VVE.TestSupport;

namespace {
using namespace vve;
constexpr unsigned count = VVE_PROBE_COUNT; ///< Capacity of one toroidal volume.
constexpr unsigned total = count * VVE_PROBE_CASCADES; ///< Work bound after the last edit, shared across volumes.
auto directory = std::filesystem::path{VVE_TEST_TMP_DIR}; ///< Distinct capture directory per subcommand.
/// @brief One full bounded sweep, including both cascades; no readback or paused GPU work hides in the renderer.
bool warm(simple::Engine &engine, unsigned frames = total / VVE_PROBE_MAX_BUDGET) { return test::renderFrames(engine,frames); }
/// @brief Disables AO in every comparison and changes GI independently of the capture fixture.
bool gi(simple::Engine &engine, bool enabled) {
    auto settings = engine.renderSystem().giSettings(); settings.enabled = enabled;
    return engine.renderSystem().setGiSettings(settings).has_value() && engine.renderFrame().has_value();
}
/// @brief Fixed narrow view keeps measurement patches inside their named surface, without moving world-anchored probes.
Camera view(Vec3 eye, Vec3 point, float fov = .25F) { return Camera::lookAt(Position{eye},Position{point},{},FovY{fov}); }
/// @brief Captures a named final PNG and reads only the projected linear HDR surface region.
std::optional<Vec3> measure(simple::Engine &engine, Vec3 point, std::string_view name) {
    auto &render = engine.renderSystem();
    if (!render.captureFrameToPng(directory / std::format("{}.png",name))) return {};
    return test::linearPoint(render.forward(),point,name);
}
/// @brief Chromatic energy cancels unchanged neutral direct illumination and neutral fallback.
float redShift(Vec3 v) { return v.x-v.y; }
/// @brief A lit red wall, black opaque roof, and neutral receiver; roof visibility is the only lighting edit.
std::optional<std::pair<RenderObjectHandle,RenderObjectHandle>> shadowScene(simple::Engine &engine) {
    auto &r = engine.renderSystem();
    r.setBackgroundColor(LinearColor{Vec3{.005F}});
    r.setPointLight(Position{},LinearColor{},LightIntensity{0},LightRange{1},LinearColor{Vec3{.005F}});
    r.setDirectionalLight(Direction{Vec3{-1,-.5F,0}},LinearColor{},LightIntensity{4},LinearColor{Vec3{0}});
    const auto wall = r.addCuboid(Vec3{-2.2F,0,-2},Vec3{-2,3,2},LinearColor{Vec3{1,.01F,.01F}});
    const auto roof = r.addCuboid(Vec3{-2.3F,3.1F,-3},Vec3{5,3.3F,3},LinearColor{Vec3{0}});
    if (!wall || !roof || !r.addPlane(Vec2{1,1},LinearColor{Vec3{.5F}})) return {};
    return std::pair{*wall,*roof};
}
/// @brief Direct-only roof reference in the main camera, run on each unchanged main-view backend.
std::optional<std::pair<Vec3,Vec3>> mainShadow(RendererBackend backend) {
    auto engine = test::hiddenEngine("cascade-main-shadow",PixelExtent{128,128}); auto &r = engine.renderSystem();
    if (!engine.init() || !r.setRendererBackend(backend) || !r.setGiSettings(GiSettings{.enabled=false})) return {};
    const auto objects = shadowScene(engine); if (!objects) return {};
    const Vec3 point{-2,1.5F,0}; r.setCamera(view(Vec3{1,1.5F,3},point));
    if (!r.setObjectVisible(objects->second,false) || !warm(engine,2)) return {};
    const auto unroofed = test::linearPoint(r.forward(),point,"main-wall-unroofed");
    if (!r.setObjectVisible(objects->second,true) || !warm(engine,2)) return {};
    const auto roofed = test::linearPoint(r.forward(),point,"main-wall-roofed");
    r.waitIdle(); engine.gui().shutdownVulkan(); r.shutdown();
    if (!unroofed || !roofed || r.forward().validationErrorCount()) return {};
    return std::pair{*unroofed,*roofed};
}
/// @brief Off-frustum wall bounce must follow a roof that only shadows it from the sun, using independent capture depth.
bool shadowed(simple::Engine &engine) {
    const auto original = mainShadow(RendererBackend::deferred), hybrid = mainShadow(RendererBackend::hybrid_gi);
    if (!original || !hybrid) return false;
    const auto directDelta = std::max(test::rgbDelta(original->first,hybrid->first),test::rgbDelta(original->second,hybrid->second));
    const float reference = redShift(original->second)/redShift(original->first);
    // The main-view reference includes the existing 0.35 shadow floor. 0.15 allows finite-face integration and penumbrae.
    const float limit = reference + .15F;
    auto &r = engine.renderSystem(); const auto objects = shadowScene(engine); if (!objects) return false;
    const Vec3 point{0,0,.5F}; r.setCamera(view(Vec3{0,1.5F,3},point));
    if (!r.setShadowDistance(1) || !r.setObjectVisible(objects->second,false) || !warm(engine)) return false;
    auto &renderer = r.forward(); auto &target = renderer.targets.front();
    const auto unroofed = measure(engine,point,"off-view-unroofed");
    // Prove the contributor is outside ALL main directional matrices, while inside the dedicated probe projection.
    unsigned covered{};
    for (auto cascade : std::views::iota(0U,unsigned(VVE_NUM_SHADOW_CASCADES))) {
        const auto clip = math::multiply(renderer.frameUniforms().shadowViewProjs[VVE_SHADOW_MATRIX_DIR_BASE+cascade],Vec4{-2,1.5F,0,1});
        covered += std::abs(clip.x/clip.w)<=1 && std::abs(clip.y/clip.w)<=1 && clip.z/clip.w>=0 && clip.z/clip.w<=1;
    }
    const auto initialRenders = target.probeShadowRenders;
    if (!warm(engine,4) || target.probeShadowRenders != initialRenders || !r.setObjectVisible(objects->second,true) || !warm(engine)) return false;
    const auto roofed = measure(engine,point,"off-view-roofed");
    if (!unroofed || !roofed) return false;
    const float ratio = redShift(*roofed)/redShift(*unroofed);
    bool pass = redShift(*unroofed)>.005F && reference<.5F && ratio<limit && limit<.65F && directDelta==0 && covered==0 && target.probeShadowRenders==initialRenders+1;
    // Intensity/weather do not change opaque sun depth; caster motion, sun direction and scrolling do.
    const auto afterRoof = target.probeShadowRenders;
    r.setDirectionalLight(Direction{Vec3{-1,-.5F,0}},LinearColor{},LightIntensity{3},LinearColor{Vec3{0}});
    r.setBackgroundColor(LinearColor{Vec3{.01F}});
    const auto sky = test::writeFixture(directory,"sky.ppm",std::string{"P6\n1 1\n255\n"}+std::string{"\x20\x20\x20",3});
    if (!r.setObjectColor(objects->first,LinearColor{Vec3{.9F,.01F,.01F}}) ||
        !r.setObjectSurface(objects->first,SurfaceMaterial{.roughness=.9F}) || !r.setSkybox(sky)) return false;
    if (!warm(engine,2)) return false;
    pass &= target.probeShadowRenders==afterRoof;
    if (!r.setObjectTransform(objects->second,Transform{.translation=Position{Vec3{0,1,0}}}) || !warm(engine,1)) return false;
    pass &= target.probeShadowRenders==afterRoof+1;
    r.setDirectionalLight(Direction{Vec3{-1,-.6F,0}},LinearColor{},LightIntensity{3},LinearColor{Vec3{0}});
    if (!warm(engine,1)) return false;
    pass &= target.probeShadowRenders==afterRoof+2;
    std::println("shadow.reference_fraction={} shadow.allowed_fraction={} shadow.off_view_fraction={} shadow.main_view.delta={} shadow.main_cascades.cover_wall={} shadow.cached.renders={} pass={}",
        reference,limit,ratio,directDelta,covered,target.probeShadowRenders,pass);
    return pass;
}
/// @brief Large nearby colored wall transports light to a receiver eleven metres from the eye, beyond near coverage.
bool coarseBounce(simple::Engine &engine) {
    auto &r = engine.renderSystem(); const Vec3 point{0,0,-8};
    r.setCamera(view(Vec3{0,1.5F,3},point));
    r.setBackgroundColor(LinearColor{Vec3{.02F}});
    r.setPointLight(Position{},LinearColor{},LightIntensity{0},LightRange{1},LinearColor{Vec3{.02F}});
    r.setDirectionalLight(Direction{Vec3{-1,0,0}},LinearColor{},LightIntensity{4},LinearColor{Vec3{0}});
    const auto wall = r.addCuboid(Vec3{-3.2F,0,-12},Vec3{-3,7,-4},LinearColor{Vec3{1,.01F,.01F}});
    if (!wall || !r.addPlane(Vec2{2,2},LinearColor{Vec3{.5F}},Transform{.translation=Position{point}}) || !gi(engine,false)) return false;
    const auto off = measure(engine,point,"coarse-off");
    if (!gi(engine,true) || !warm(engine)) return false;
    const auto red = measure(engine,point,"coarse-red");
    auto &renderer = r.forward(); renderer.debugProbeCoarse=false;
    if (!warm(engine,1)) return false;
    const auto disabled = measure(engine,point,"coarse-internal-disabled");
    renderer.debugProbeCoarse=true;
    if (!r.setObjectColor(*wall,LinearColor{Vec3{.01F,1,.01F}}) || !warm(engine)) return false;
    const auto green = measure(engine,point,"coarse-green");
    if (!off || !red || !green || !disabled) return false;
    const auto fallbackDelta = test::rgbDelta(*off,*disabled);
    const auto &target = renderer.targets.front();
    bool outside = false;
    for (const int axis : {0,1,2}) outside |= point[axis]<target.probes.origin[axis] || point[axis]>target.probes.origin[axis]+(VVE_PROBE_SIDE-1)*VVE_PROBE_SPACING;
    std::println("coarse.receiver.distance={} coarse.receiver.outside_near={} coarse.red_shift={} coarse.green_shift={} coarse.shift.minimum=0.005 coarse.fallback.delta={} coarse.fallback.limit=0",
        math::length(math::subtract(point,Vec3{0,1.5F,3})),outside,redShift(*red),-redShift(*green),fallbackDelta);
    return outside && redShift(*red)>.005F && -redShift(*green)>.005F && red->x>off->x+.005F && green->y>off->y+.005F && fallbackDelta==0;
}
/// @brief Samples one uniform neutral floor through both transition bands, with a fixed probe origin and HDR exposure.
bool continuity(simple::Engine &engine) {
    auto &r = engine.renderSystem();
    r.clearLights(); r.setBackgroundColor(LinearColor{Vec3{.2F}});
    r.setPointLight(Position{},LinearColor{},LightIntensity{0},LightRange{1},LinearColor{Vec3{.1F}});
    const Vec3 eye{0,1.5F,3}; r.setCamera(view(eye,Vec3{0,0,0}));
    if (!r.addPlane(Vec2{300,300},LinearColor{Vec3{.5F}}) || !warm(engine)) return false;
    constexpr float spacing=.25F, tolerance=.002F; // Quarter-metre samples resolve the 2 m near fade by eight steps.
    std::vector<float> values;
    for (auto sample : std::views::iota(0U,81U)) {
        const Vec3 point{0,0,-spacing*sample}; r.setCamera(view(eye,point,.1F));
        if (!warm(engine,1)) return false;
        const auto value = test::linearPoint(r.forward(),point,std::format("continuity-{}",sample));
        if (!value || !std::isfinite(value->x) || !std::isfinite(value->y) || !std::isfinite(value->z) || std::min({value->x,value->y,value->z})<=.001F) return false;
        values.push_back(value->x);
    }
    if (!r.captureFrameToPng(directory/"continuity.png")) return false;
    const float difference=std::abs(values.front()-values.back());
    // Twice the endpoint linear ramp slope covers interpolation/quadrature; the absolute 0.002 is the existing HDR noise tolerance.
    const float limit=2*difference*spacing/(VVE_PROBE_SPACING*VVE_PROBE_BLEND_CELLS)+tolerance;
    const float minimum=*std::ranges::min_element(values), floor=std::min(values.front(),values.back())-tolerance;
    float step{};
    for (auto i : std::views::iota(std::size_t{1},values.size())) step=std::max(step,std::abs(values[i]-values[i-1]));
    std::println("continuity.spacing={} continuity.endpoints={},{} continuity.endpoint_delta={} continuity.max_step={} continuity.step_limit={} continuity.minimum={} continuity.minimum_limit={} continuity.samples={}",
        spacing,values.front(),values.back(),difference,step,limit,minimum,floor,values.size());
    return step<limit && minimum>=floor;
}
/// @brief Reads one small coefficient image only at test checkpoints; all overlap texels can then be compared byte for byte.
std::optional<std::vector<std::byte>> coefficients(simple::ForwardRenderer &renderer, const simple::IrradianceProbes &p) {
    simple::VulkanReadback readback;
    if (readback.create(renderer.allocator,renderer.device.device,renderer.device.graphicsQueue,renderer.commandPool.commandPool,
        p.coefficients.extent,VK_FORMAT_R32G32B32A32_SFLOAT)!=VK_SUCCESS ||
        readback.capture(p.coefficients.image,0,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)!=VK_SUCCESS) return {};
    const auto bytes=readback.pixelBytes(); std::vector<std::byte> result(bytes.size()); std::memcpy(result.data(),bytes.data(),bytes.size()); return result;
}
/// @brief A coarse-cell move clears its exposed slab, preserves overlap exactly, and shares recapture with missing near cells.
bool scroll(simple::Engine &engine) {
    auto &r=engine.renderSystem(); r.clearLights(); r.setBackgroundColor(LinearColor{Vec3{.2F}});
    r.setCamera(view(Vec3{0,1.5F,3},Vec3{0,0,0}));
    if (!warm(engine)) return false;
    auto &renderer=r.forward(); auto &target=renderer.targets.front(); auto &p=target.coarseProbes;
    const auto before=coefficients(renderer,p); const auto ages=p.captured;
    if (!before) return false;
    r.setCamera(view(Vec3{VVE_PROBE_SPACING*VVE_PROBE_SCALE(1),1.5F,3},Vec3{0,0,0}));
    if (!warm(engine,1)) return false;
    const auto after=coefficients(renderer,p); const auto validity=test::probeValidity(renderer,1);
    if (!after || !validity) return false;
    constexpr unsigned slab=VVE_PROBE_SIDE*VVE_PROBE_SIDE, overlap=count-slab;
    unsigned retained{};
    for (auto row : std::views::iota(0U,count)) {
        if (!p.captured[row]) continue;
        const auto offset=row*simple::IrradianceProbes::atlasColumns*16U;
        if (p.captured[row]!=ages[row] || std::memcmp(before->data()+offset,after->data()+offset,simple::IrradianceProbes::atlasColumns*16U)) return false;
        ++retained;
    }
    constexpr unsigned bound=(count+slab+VVE_PROBE_MAX_BUDGET-1)/VVE_PROBE_MAX_BUDGET;
    unsigned frames=1;
    while ((target.probes.valid<count || p.valid<count) && frames<=bound) { if (!warm(engine,1)) return false; ++frames; }
    const auto complete=test::probeValidity(renderer,1);
    std::println("scroll.coarse.overlap={} scroll.expected_overlap={} scroll.first.valid={} scroll.convergence.frames={} scroll.convergence.bound={}",retained,overlap,validity->first,frames,bound);
    return retained==overlap && validity->first==overlap && complete && complete->first==count && frames<=bound;
}
/// @brief Teleports discard both volumes before any lighting; every receiver channel must equal the explicit disabled fallback.
bool teleport(simple::Engine &engine) {
    auto &r=engine.renderSystem(); r.clearLights(); r.setBackgroundColor(LinearColor{Vec3{.2F}});
    r.setPointLight(Position{},LinearColor{},LightIntensity{0},LightRange{1},LinearColor{Vec3{.02F}});
    r.setCamera(view(Vec3{0,1.5F,3},Vec3{0,0,0}));
    if (!r.addPlane(Vec2{100,100},LinearColor{Vec3{.5F}}) || !warm(engine)) return false;
    r.setCamera(view(Vec3{48,1.5F,3},Vec3{48,0,0})); if (!warm(engine,1)) return false;
    auto &renderer=r.forward(); const auto &target=renderer.targets.front();
    const auto near=test::probeValidity(renderer), coarse=test::probeValidity(renderer,1);
    const bool empty=near && coarse && near->first==0 && near->second==0 && coarse->first==0 && coarse->second==0 && target.probes.updated+target.coarseProbes.updated==0;
    // PNG capture acquires another frame. Read HDR directly here so the exact first post-teleport frame is tested.
    const auto reset=test::linearPoint(renderer,Vec3{48,0,0},"teleport-first");
    if (!gi(engine,false)) return false;
    const auto off=measure(engine,Vec3{48,0,0},"teleport-fallback");
    if (!reset || !off) return false;
    const auto delta=test::rgbDelta(*reset,*off);
    if (!gi(engine,true) || !warm(engine)) return false;
    std::println("teleport.near.valid={} teleport.coarse.valid={} teleport.fallback.delta={} teleport.fallback.limit=0",near ? near->first : count,coarse ? coarse->first : count,delta);
    return empty && delta==0 && target.probes.valid==count && target.coarseProbes.valid==count;
}
/// @brief Measures every allocation and actual scheduling frame, testing both presets against the exact payload formula and cap.
bool memory(simple::Engine &engine, bool low) {
    auto &r=engine.renderSystem(); auto settings=r.giSettings(); settings.quality=low ? GiQuality::low : GiQuality::medium;
    if (!r.setGiSettings(settings)) return false;
    r.clearLights(); r.setBackgroundColor(LinearColor{Vec3{.2F}}); r.setCamera(view(Vec3{0,1.5F,3},Vec3{}));
    const unsigned budget=low ? 1 : VVE_PROBE_MAX_BUDGET, bound=(total+budget-1)/budget;
    unsigned frames{};
    do {
        if (!warm(engine,1)) return false;
        ++frames;
        const auto &t=r.forward().targets.front();
        if (t.probes.updated+t.coarseProbes.updated!=budget) return false;
        if (t.probes.valid+t.coarseProbes.valid==total) break;
    } while (frames<=bound);
    auto &renderer=r.forward(); const auto &t=renderer.targets.front();
    const auto near=test::probeValidity(renderer), coarse=test::probeValidity(renderer,1);
    constexpr std::uint64_t faceBytes=VVE_POINT_SHADOW_FACES*VVE_PROBE_FACE_SIZE*VVE_PROBE_MAX_BUDGET*VVE_PROBE_FACE_SIZE;
    constexpr std::uint64_t volume=faceBytes*(8+4)+VVE_PROBE_ATLAS_COLUMNS*count*16ULL+
        simple::WindowTarget::framesInFlight*simple::IrradianceProbes::views*sizeof(simple::FrameUniforms);
    constexpr std::uint64_t shadow=VVE_PROBE_SHADOW_RESOLUTION*VVE_PROBE_SHADOW_RESOLUTION*4ULL, formula=VVE_PROBE_CASCADES*volume+shadow, cap=2*1024*1024;
    const auto bytes=t.probes.gpuBytes+t.coarseProbes.gpuBytes+t.probeShadowBytes;
    std::println("gi.quality={} gi.convergence.frames={} gi.convergence.bound={} gi.memory.formula={} gi.memory.cap={} gi.near.formula={} gi.frame_uniform_bytes={} gi.gpu_bytes={}",
        low ? "low" : "medium",frames,bound,formula,cap,volume,sizeof(simple::FrameUniforms),bytes);
    return near && coarse && near->first==count && coarse->first==count && frames<=bound && t.probes.gpuBytes==319232 &&
        t.probes.gpuBytes==volume && t.coarseProbes.gpuBytes==volume && t.probeShadowBytes==shadow && bytes==formula && bytes<=cap;
}
}
/// @brief Every registered subcommand uses the named Vulkan device, validation through teardown, deterministic fixtures and explicit receipts.
int main(int argc, char **argv) {
    const std::string_view mode=argc==1 ? "shadowed-capture" : std::string_view{argv[1]};
    const std::array modes{"shadowed-capture","coarse-bounce","continuity","coarse-scroll","teleport","memory","memory-low"};
    if (argc>2 || std::ranges::find(modes,mode)==modes.end()) return 3;
    directory/=mode; std::filesystem::create_directories(directory);
    auto engine=test::hiddenEngine("probe-cascades",PixelExtent{128,128}); auto &r=engine.renderSystem();
    if (!engine.init() || !r.setRendererBackend(RendererBackend::hybrid_gi) || !r.setAnimationTime(AnimationTime{})) return 1;
    const bool pass=mode=="shadowed-capture" ? shadowed(engine) : mode=="coarse-bounce" ? coarseBounce(engine) : mode=="continuity" ? continuity(engine) :
        mode=="coarse-scroll" ? scroll(engine) : mode=="teleport" ? teleport(engine) : memory(engine,mode=="memory-low");
    auto &renderer=r.forward(); if (renderer.targets.empty()) return 1;
    VkPhysicalDeviceProperties device{}; vkGetPhysicalDeviceProperties(renderer.physicalDevice.physicalDevice,&device);
    const bool validation=renderer.validationActive();
    const auto near=test::probeValidity(renderer), coarse=test::probeValidity(renderer,1);
    r.waitIdle(); engine.gui().shutdownVulkan(); r.shutdown();
    std::println("renderer.backend=hybrid-gi device={} mode={} validation_active={} validation.errors={} pass={} captures={}",
        std::string_view{device.deviceName},mode,validation,renderer.validationErrorCount(),pass,directory.string());
#ifndef NDEBUG
    if (!validation) return 2;
#endif
    return pass && near && coarse && renderer.validationErrorCount()==0 ? 0 : 2;
}
