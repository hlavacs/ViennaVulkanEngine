/** @file @brief Linear-HDR transparency transport, nonblocking capture, opaque depth and ACES ordering receipts. */
#include <vulkan/vulkan_core.h>
#include <VVPPL.h>
import std;
import VVEngine;
import VVEngine.Simple;
import VVEngine.Simple.Renderer;
import VVE.TestSupport;

namespace {
using namespace vve;
auto directory=std::filesystem::path{VVE_TEST_TMP_DIR}; ///< Subcommands retain separate captures.
constexpr Vec3 red{1,.01F,.01F}, green{.01F,1,.01F}; ///< The only colored radiance source in this fixture.
/// @brief Controls probe contribution independently of AO; two sweeps converge the fixed 64-probe volume.
bool gi(simple::Engine &engine,bool enabled,float strength=1) {
    auto settings=engine.renderSystem().giSettings(); settings.enabled=enabled; settings.indirect_strength=strength;
    return engine.renderSystem().setGiSettings(settings).has_value() && test::renderFrames(engine,enabled ? 64 : 4);
}
/// @brief A horizontal pane has zero direct cosine and blends over black unlit geometry, isolating its own indirect light.
std::optional<std::pair<RenderObjectHandle,RenderObjectHandle>> paneScene(simple::Engine &engine) {
    auto &render=engine.renderSystem();
    const auto floor=render.addPlane(Vec2{1,1},LinearColor{.value=Vec3{0}});
    const auto pane=render.addPlane(Vec2{.7F,.7F},LinearColor{.value=Vec3{.5F}},Transform{.translation=Position{.value=Vec3{0,.2F,0}}});
    const auto wall=render.addCuboid(Vec3{-2.2F,0,-2},Vec3{-2,3,2},LinearColor{.value=red});
    if (!floor || !pane || !wall || !render.setObjectUnlit(*floor,true) || !render.setObjectOpacity(*pane,.5F)) return {};
    return std::pair{*pane,*wall};
}
/// @brief This four-pixel region sees the transparent material over the unlit background, never an opaque GI receiver.
std::optional<Vec3> paneValue(simple::Engine &engine,std::string_view name) {
    return test::linearPoint(engine.renderSystem().forward(),Vec3{0,.2F,0},name);
}
/// @brief Recoloring an off-screen wall must change pane hue; neither an ambient constant nor lit background can pass.
bool color(simple::Engine &engine) {
    auto &render=engine.renderSystem(); const auto scene=paneScene(engine);
    if (!scene || !gi(engine,false)) return false;
    const auto off=paneValue(engine,"pane-off");
    if (!gi(engine,true,.5F)) return false;
    const auto half=paneValue(engine,"pane-half");
    if (!gi(engine,true)) return false;
    const auto on=paneValue(engine,"pane-red");
    if (!render.captureFrameToPng(directory/"red.png")) return false;
    auto settings=render.giSettings(); settings.ao_enabled=true;
    if (!render.setGiSettings(settings) || !test::renderFrames(engine,4)) return false;
    const auto ao=paneValue(engine,"pane-red-ao");
    if (!render.setObjectColor(scene->second,LinearColor{.value=green}) || !test::renderFrames(engine)) return false;
    const auto recolored=paneValue(engine,"pane-green");
    if (!render.captureFrameToPng(directory/"green.png") || !off || !half || !on || !ao || !recolored) return false;
    const Vec3 redDelta=math::subtract(*on,*off), greenDelta=math::subtract(*recolored,*off);
    const float linearity=test::rgbDelta(math::subtract(*half,*off),math::scale(redDelta,.5F));
    const float aoError=test::rgbDelta(*on,*ao);
    std::println("transparent.red_delta={},{},{} transparent.green_delta={},{},{} color.minimum=0.003 color.dominance=1.5 strength.error={} strength.limit=0.001 transparent.ao_delta={} transparent.ao.limit=0.0005",
        redDelta.x,redDelta.y,redDelta.z,greenDelta.x,greenDelta.y,greenDelta.z,linearity,aoError);
    return redDelta.x>.003F && redDelta.x>1.5F*redDelta.y && greenDelta.y>.003F && greenDelta.y>1.5F*greenDelta.x &&
        linearity<.001F && aoError<.0005F;
}
/// @brief The separating pane is out of the main receiver region; red-minus-black isolates transported light in each state.
bool blocking(simple::Engine &engine) {
    auto &render=engine.renderSystem();
    const Vec3 point{-.5F,0,1};
    render.setCamera(Camera::lookAt(Position{.value=Vec3{.2F,1.5F,3}},Position{.value=point},{},FovY{1.1F}));
    const auto floor=render.addPlane(Vec2{8,8},LinearColor{.value=Vec3{.5F}});
    const auto pane=render.addCuboid(Vec3{-1.1F,-3,-5},Vec3{-.9F,4,5},LinearColor{.value=Vec3{0}});
    const auto wall=render.addCuboid(Vec3{-3.2F,0,-4},Vec3{-3,3,4},LinearColor{.value=red});
    if (!floor || !pane || !wall || !gi(engine,true)) return false;
    std::array<float,3> bounce{};
    const std::array names{"no-pane","transparent-pane","opaque-pane"};
    for (auto state:std::views::iota(0U,3U)) {
        if (!render.setObjectVisible(*pane,state!=0) || !render.setObjectOpacity(*pane,state==1 ? .5F : 1.0F)) return false;
        if (!render.setObjectColor(*wall,LinearColor{.value=red}) || !test::renderFrames(engine)) return false;
        const auto lit=test::linearPoint(render.forward(),point,std::format("{}-red",names[state]));
        if (!render.captureFrameToPng(directory/std::format("{}.png",names[state])) ||
            !render.setObjectColor(*wall,LinearColor{.value=Vec3{0}}) || !test::renderFrames(engine)) return false;
        const auto black=test::linearPoint(render.forward(),point,std::format("{}-black",names[state]));
        if (!lit || !black) return false; bounce[state]=lit->x-black->x;
    }
    const float retained=bounce[1]/bounce[0], opaque=bounce[2]/bounce[0];
    std::println("blocking.no_pane={} blocking.transparent={} blocking.opaque={} blocking.retained={} blocking.retained.minimum=0.95 blocking.opaque_fraction={} blocking.opaque.maximum=0.30",bounce[0],bounce[1],bounce[2],retained,opaque);
    return bounce[0]>.005F && retained>=.95F && opaque<.30F;
}
/// @brief Opaque geometry in front of a transparent pane must hide it even though the pane is drawn in a later pass.
bool ordering(simple::Engine &engine) {
    auto &render=engine.renderSystem(); const auto scene=paneScene(engine);
    const auto foreground=render.addPlane(Vec2{.8F,.8F},LinearColor{.value=Vec3{.2F,.4F,.1F}},Transform{.translation=Position{.value=Vec3{0,.6F,0}}});
    if (!scene || !foreground || !render.setObjectUnlit(*foreground,true) || !render.setObjectVisible(scene->first,false) || !gi(engine,true)) return false;
    const auto off=test::linearPoint(render.forward(),Vec3{0,.6F,0},"ordering-without-pane");
    if (!render.setObjectVisible(scene->first,true) || !test::renderFrames(engine,4)) return false;
    const auto on=test::linearPoint(render.forward(),Vec3{0,.6F,0},"ordering-hidden-pane");
    const auto passes=render.forward().lastRecordedPassOrder(); using Pass=simple::ForwardRenderer::RecordedPass;
    const bool order=std::ranges::find(passes,Pass::gbuffer)<std::ranges::find(passes,Pass::deferred_lighting) &&
        std::ranges::find(passes,Pass::deferred_lighting)<std::ranges::find(passes,Pass::forward_transparency);
    if (!off || !on || !render.captureFrameToPng(directory/"opaque-depth.png")) return false;
    const float error=test::rgbDelta(*off,*on);
    std::println("ordering.hdr_delta={} ordering.limit=0 ordering.pass_sequence={} ordering.reference_error={}",error,order,test::rgbDelta(*on,Vec3{.2F,.4F,.1F}));
    return error==0 && order && test::rgbDelta(*on,Vec3{.2F,.4F,.1F})<.001F;
}
/// @brief Mirrors the existing VVPPL ACES reference curve only in this numerical test, then decodes the sRGB PNG.
float aces(float x) { return std::clamp(x*(2.51F*x+.03F)/(x*(2.43F*x+.59F)+.14F),0.0F,1.0F); }
/// @brief Measures the same projected pane region in a final display capture, decoded back to linear display light.
std::optional<Vec3> display(simple::Engine &engine,const std::filesystem::path &path) {
    if (!engine.renderSystem().captureFrameToPng(path)) return {};
    const auto pixels=test::imagePixels(path); if (!pixels || pixels->size()!=128*128*4) return {};
    const auto &f=engine.renderSystem().forward().frameUniforms();
    const auto p=math::multiply(math::multiply(f.projection,f.view),Vec4{0,.2F,0,1});
    const int x=int((p.x/p.w*.5F+.5F)*128)-1,y=int((p.y/p.w*.5F+.5F)*128)-1;
    Vec3 result{};
    // PNG quantization is the only display tolerance; compare decoded ACES output rather than byte counts.
    for (auto row:std::views::iota(y,y+2)) for (auto column:std::views::iota(x,x+2)) {
        const auto *pixel=test::pixelAt(pixels->data(),128,column,row);
        for (auto c:std::views::iota(0U,3U)) {
            const float v=float(pixel[c])/255; result[c]+=(v<=.04045F ? v/12.92F : std::pow((v+.055F)/1.055F,2.4F))*.25F;
        }
    }
    return result;
}
/// @brief Both GI states must obey ACES(exposure * HDR); exposure cannot alter the upstream pane radiance.
bool tonemap(simple::Engine &engine,vvppl::TonemapSettings *&tone) {
    if (!paneScene(engine)) return false;
    std::array<Vec3,2> first{}; float maximum{}, hdrError{}, displayChange{};
    for (auto index:std::views::iota(0U,2U)) {
        // The first frame creates the post-process chain through its normal setup callback.
        if (!test::renderFrames(engine,4) || !tone) return false;
        const float exposure=index==0 ? .5F : 2.0F; tone->exposure=exposure;
        std::array<Vec3,2> ldr{};
        for (auto enabled:std::views::iota(0U,2U)) {
            if (!gi(engine,enabled!=0)) return false;
            const auto hdr=paneValue(engine,std::format("exposure-{}-gi-{}",exposure,enabled));
            const auto value=display(engine,directory/std::format("exposure-{}-gi-{}.png",exposure,enabled));
            if (!hdr || !value) return false;
            Vec3 expected{}; for (auto c:std::views::iota(0U,3U)) expected[c]=aces((*hdr)[c]*exposure);
            const float error=test::rgbDelta(*value,expected); maximum=std::max(maximum,error);
            if (index==0) first[enabled]=*hdr; else hdrError=std::max(hdrError,test::rgbDelta(first[enabled],*hdr));
            ldr[enabled]=*value;
            std::println("exposure={} gi.enabled={} pane.hdr={},{},{} pane.ldr_linear={},{},{} aces.expected={},{},{} aces.error={}",exposure,enabled,hdr->x,hdr->y,hdr->z,value->x,value->y,value->z,expected.x,expected.y,expected.z,error);
        }
        displayChange=std::max(displayChange,test::rgbDelta(ldr[1],ldr[0]));
    }
    const auto passes=engine.renderSystem().forward().lastRecordedPassOrder(); using Pass=simple::ForwardRenderer::RecordedPass;
    const bool before=std::ranges::find(passes,Pass::forward_transparency)<std::ranges::find(passes,Pass::post_process) && std::ranges::find(passes,Pass::post_process)!=passes.end();
    std::println("tonemap.aces_error={} tonemap.aces.limit=0.008 tonemap.hdr_exposure_delta={} tonemap.hdr.limit=0.0005 tonemap.gi_before_post={} tonemap.display_change={} tonemap.gi_hdr_delta={}",maximum,hdrError,before,displayChange,test::rgbDelta(first[1],first[0]));
    return maximum<.008F && hdrError<.0005F && before && displayChange>.003F && first[1].x>first[0].x+.003F;
}
} // namespace
/// @brief Registered subcommands exercise actual scene composition at one or four coverage samples with validation enabled.
int main(int argc,char **argv) {
    using namespace vve;
    const std::string_view mode=argc==2 ? argv[1] : "color";
    const std::array modes{"color","color-msaa4","blocking","ordering","tonemap"};
    if (argc>2 || std::ranges::find(modes,mode)==modes.end()) return 3;
    directory/=mode; std::filesystem::create_directories(directory);
    auto engine=test::hiddenEngine("gi-transparency",PixelExtent{128,128}); auto &render=engine.renderSystem();
    if (!engine.init() || !render.setRendererBackend(RendererBackend::hybrid_gi) || !render.setAnimationTime(AnimationTime{}) ||
        !render.setAntialiasing(mode=="color-msaa4" ? Antialiasing::msaa4 : Antialiasing::none)) return 1;
    auto camera=Camera::lookAt(Position{.value=Vec3{0,1.5F,3}},Position{}); camera.fov_y.radians=1.1F; render.setCamera(camera);
    render.setBackgroundColor(LinearColor{.value=Vec3{0}});
    render.setDirectionalLight(Direction{.value=Vec3{-1,0,0}},LinearColor{},LightIntensity{4},LinearColor{.value=Vec3{0}});
    render.setPointLight(Position{},LinearColor{},LightIntensity{0},LightRange{1},LinearColor{.value=Vec3{0}});
    vvppl::TonemapSettings *tone{};
    if (mode=="tonemap") render.setPostProcessSetup([&](vvppl::PostProcessing &pp) { tone=&pp.addTonemap(); });
    bool pass=mode=="blocking" ? blocking(engine) : mode=="ordering" ? ordering(engine) : mode=="tonemap" ? tonemap(engine,tone) : color(engine);
    auto &renderer=render.forward(); VkPhysicalDeviceProperties device{}; vkGetPhysicalDeviceProperties(renderer.physicalDevice.physicalDevice,&device);
    std::println("device={} samples={} mode={} validation_active={} gi.near.gpu_bytes={}",std::string_view{device.deviceName},unsigned(renderer.sampleCount()),mode,renderer.validationActive(),renderer.targets.front().probes.gpuBytes);
#ifndef NDEBUG
    pass &= renderer.validationActive();
#endif
    pass &= renderer.targets.front().probes.gpuBytes==319232;
    render.waitIdle(); engine.gui().shutdownVulkan(); render.shutdown();
    std::println("validation.errors={} test.pass={}",renderer.validationErrorCount(),pass);
    return pass && renderer.validationErrorCount()==0 ? 0 : 2;
}
