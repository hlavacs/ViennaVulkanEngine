/** @file @brief Native linear-HDR receipts for hybrid masks, sidedness, metals, strength and sRGB transport. */
#include <vulkan/vulkan_core.h>
import std;
import VVEngine;
import VVEngine.Simple;
import VVEngine.Simple.Renderer;
import VVE.TestSupport;

namespace {
using namespace vve;
auto directory = std::filesystem::path{VVE_TEST_TMP_DIR}; ///< Per-subcommand retained glTF fixtures and captures.
const std::array wallPoints{Vec3{-2,0,2},Vec3{-2,0,-2},Vec3{-2,3,-2},Vec3{-2,3,2}}; ///< Front faces the receiver (+X).
constexpr std::string_view red = R"({"pbrMetallicRoughness":{"baseColorFactor":[1,0.01,0.01,1],"metallicFactor":0,"roughnessFactor":1}})";
constexpr std::string_view masked = R"({"doubleSided":true,"alphaMode":"MASK","alphaCutoff":0.7,"pbrMetallicRoughness":{"baseColorFactor":[1,0.01,0.01,1],"baseColorTexture":{"index":0},"metallicFactor":0,"roughnessFactor":1}})";

/// @brief Imports an actual glTF material on a quad through the normal asset path, including alpha and sidedness.
std::optional<RenderObjectHandle> materialQuad(simple::Engine &engine, const std::filesystem::path &directory,
	std::string_view name, const std::array<Vec3,4> &positions, std::string_view material, bool reverse = false) {
	std::filesystem::create_directories(directory);
	const auto normal = math::scale(math::normalize(math::cross(math::subtract(positions[1],positions[0]),
		math::subtract(positions[2],positions[0]))), reverse ? -1.0F : 1.0F);
	std::array<float,32> attributes{};
	const std::array uv{Vec2{0,0},Vec2{1,0},Vec2{1,1},Vec2{0,1}};
	// Separate glTF attribute ranges keep this small fixture independent of private renderer layouts.
	for (auto i : std::views::iota(0U,4U)) {
		for (auto axis : std::views::iota(0U,3U)) { attributes[3*i+axis] = positions[i][axis]; attributes[12+3*i+axis] = normal[axis]; }
		attributes[24+2*i] = uv[i].x; attributes[25+2*i] = uv[i].y;
	}
	const std::array<std::uint16_t,6> indices = reverse ? std::array<std::uint16_t,6>{0,2,1,0,3,2} : std::array<std::uint16_t,6>{0,1,2,0,2,3};
	const auto binaryName = std::format("{}.bin",name);
	{
		std::ofstream binary{directory / binaryName,std::ios::binary};
		binary.write(reinterpret_cast<const char *>(attributes.data()),sizeof(attributes));
		binary.write(reinterpret_cast<const char *>(indices.data()),sizeof(indices));
	}
	const auto json = std::string{R"({"asset":{"version":"2.0"},"buffers":[{"uri":")"} + binaryName + R"(","byteLength":140}],
	 "bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":48},{"buffer":0,"byteOffset":48,"byteLength":48},
	 {"buffer":0,"byteOffset":96,"byteLength":32},{"buffer":0,"byteOffset":128,"byteLength":12}],
	 "accessors":[{"bufferView":0,"componentType":5126,"count":4,"type":"VEC3","min":[-20,-20,-20],"max":[20,20,20]},
	 {"bufferView":1,"componentType":5126,"count":4,"type":"VEC3"},{"bufferView":2,"componentType":5126,"count":4,"type":"VEC2"},
	 {"bufferView":3,"componentType":5123,"count":6,"type":"SCALAR"}],
	 "images":[{"uri":"mask.tga"}],"samplers":[{"magFilter":9728,"minFilter":9728}],"textures":[{"source":0,"sampler":0}],"materials":[)" +
	 std::string{material} + R"(],"meshes":[{"primitives":[{"attributes":{"POSITION":0,"NORMAL":1,"TEXCOORD_0":2},"indices":3,"material":0}]}],
	 "nodes":[{"mesh":0}],"scenes":[{"nodes":[0]}],"scene":0})";
	const auto scene = engine.assets().loadScene(test::writeFixture(directory,std::format("{}.gltf",name),json));
	if (!scene) return {};
	const auto instance = engine.renderSystem().instantiateScene(*scene);
	if (!instance) return {};
	const auto objects = engine.renderSystem().sceneInstanceObjects(*instance);
	return objects && objects->size() == 1 ? std::optional{objects->front()} : std::nullopt;
}

/// @brief An uncompressed RGBA TGA has four-square checker holes; alpha 153 tests an imported nondefault cutoff (0.7).
void maskTexture() {
    std::string bytes(18,'\0'); bytes[2] = 2; bytes[12] = 64; bytes[14] = 64; bytes[16] = 32; bytes[17] = 0x28;
    unsigned holes{};
    for (auto y : std::views::iota(0U,64U)) for (auto x : std::views::iota(0U,64U)) {
        const bool hole = ((x/16+y/16)&1) != 0; holes += hole;
        bytes += std::string(3,char(255)); bytes += char(hole ? 153 : 255);
    }
    (void)test::writeFixture(directory,"mask.tga",bytes);
    std::println("mask.texels=4096 mask.holes={} mask.hole_fraction={} mask.cutoff=0.7 bounce.band=0.30,0.70",holes,float(holes)/4096);
}
/// @brief Public runtime controls leave AO off and pause time; two volume sweeps remove capture scheduling from comparisons.
bool gi(simple::Engine &engine, bool enabled, float strength = 1) {
    auto settings = engine.renderSystem().giSettings(); settings.enabled = enabled; settings.indirect_strength = strength;
    return engine.renderSystem().setGiSettings(settings).has_value() && test::renderFrames(engine,enabled ? 64 : 4);
}
/// @brief Measures only the neutral horizontal receiver, which has zero cosine to the fixture's horizontal sun.
std::optional<Vec3> receiver(simple::Engine &engine, std::string_view name) {
    return test::linearPoint(engine.renderSystem().forward(),Vec3{},name);
}
/// @brief Retains the inspected display image while every acceptance decision uses linear HDR.
bool capture(simple::Engine &engine, std::string_view name) {
    return engine.renderSystem().captureFrameToPng(directory / std::format("{}.png",name)).has_value();
}
/// @brief Main holes and shadow holes are tested separately so the visible card cannot hide its own floor shadow.
bool holes(simple::Engine &engine) {
    auto &render = engine.renderSystem(); render.clearScene();
    render.setDirectionalLight(Direction{.value = Vec3{0,-1,0}},LinearColor{},LightIntensity{1},LinearColor{.value = Vec3{0}});
    if (!render.addPlane(Vec2{3,3},LinearColor{.value = Vec3{.5F}})) return false;
    const std::array points{Vec3{-1,1,1},Vec3{1,1,1},Vec3{1,1,-1},Vec3{-1,1,-1}};
    const auto card = materialQuad(engine,directory,"overhead-mask",points,masked);
    const auto solid = materialQuad(engine,directory,"overhead-solid",points,red);
    if (!card || !solid || !render.setObjectVisible(*solid,false) || !render.setObjectVisible(*card,false)) return false;
    render.setCamera(Camera::lookAt(Position{.value = Vec3{0,4,0}},Position{}));
    if (!gi(engine,false)) return false;
    // The projected ray through this card hole hits the ground directly below it.
    const Vec3 hole{-.75F,0,.25F}, opaque{-.75F,0,.75F};
    const auto background = test::linearPoint(render.forward(),hole,"main-no-card");
    if (!render.setObjectVisible(*card,true) || !test::renderFrames(engine,4)) return false;
    const auto mainHole = test::linearPoint(render.forward(),hole,"main-hole");
    const auto mainCard = test::linearPoint(render.forward(),Vec3{opaque.x,1,opaque.z},"main-opaque-texel");
    if (!capture(engine,"main-holes")) return false;
    // View the ground from below the card height; shadow regions are no longer overdrawn by the card.
    render.setCamera(Camera::lookAt(Position{.value = Vec3{0,.65F,3}},Position{}));
    if (!test::renderFrames(engine,4)) return false;
    const auto lit = test::linearPoint(render.forward(),hole,"shadow-hole");
    const auto dark = test::linearPoint(render.forward(),opaque,"shadow-opaque-texel");
    if (!capture(engine,"shadow-holes") || !render.setObjectVisible(*card,false) || !render.setObjectVisible(*solid,true) || !test::renderFrames(engine,4)) return false;
    const auto blocked = test::linearPoint(render.forward(),hole,"shadow-solid-control");
    if (!background || !mainHole || !mainCard || !lit || !dark || !blocked) return false;
    const float error = test::rgbDelta(*background,*mainHole);
    std::println("main.hole.error={} main.hole.limit=0.005 shadow.hole={} shadow.solid={} shadow.masked={} shadow.dark_fraction.limit=0.8",error,lit->x,blocked->x,dark->x);
    return error < .005F && mainCard->x > mainCard->y*2 && lit->x > .1F && blocked->x < .8F*lit->x && dark->x < .8F*lit->x;
}
/// @brief Cutout coverage must reduce real red bounce by approximately its measured 50% surface coverage.
bool cutout(simple::Engine &engine) {
    auto &render = engine.renderSystem();
    const auto solid = materialQuad(engine,directory,"solid",wallPoints,red);
    const auto card = materialQuad(engine,directory,"masked",wallPoints,masked);
    if (!solid || !card || !render.setObjectVisible(*solid,false) || !render.setObjectVisible(*card,false) || !gi(engine,true)) return false;
    const auto empty = receiver(engine,"no-wall");
    if (!render.setObjectVisible(*solid,true) || !test::renderFrames(engine)) return false;
    const auto full = receiver(engine,"solid");
    if (!capture(engine,"solid-bounce") || !render.setObjectVisible(*solid,false) || !render.setObjectVisible(*card,true) || !test::renderFrames(engine)) return false;
    const auto half = receiver(engine,"masked");
    if (!capture(engine,"masked-bounce") || !empty || !full || !half) return false;
    const float fraction = (half->x-empty->x)/(full->x-empty->x);
    std::println("bounce.empty={} bounce.solid={} bounce.masked={} bounce.fraction={} bounce.band=0.30,0.70 bounce.minimum=0.005",empty->x,full->x,half->x,fraction);
    return full->x > empty->x+.005F && fraction > .30F && fraction < .70F && holes(engine);
}
/// @brief A reversed card presents its back toward the receiver: only a two-sided material reflects light from that side.
bool sided(simple::Engine &engine) {
    auto &render = engine.renderSystem();
    const auto single = materialQuad(engine,directory,"single",wallPoints,red,true);
    const auto both = materialQuad(engine,directory,"double",wallPoints,
        R"({"doubleSided":true,"pbrMetallicRoughness":{"baseColorFactor":[1,0.01,0.01,1],"metallicFactor":0,"roughnessFactor":1}})",true);
    if (!single || !both || !render.setObjectVisible(*both,false) || !render.setObjectVisible(*single,false) || !gi(engine,true)) return false;
    const auto empty = receiver(engine,"no-card");
    if (!render.setObjectVisible(*single,true) || !test::renderFrames(engine)) return false;
    const auto a = receiver(engine,"single-back");
    if (!capture(engine,"single-back") || !render.setObjectVisible(*single,false) || !render.setObjectVisible(*both,true) || !test::renderFrames(engine)) return false;
    const auto b = receiver(engine,"double-back");
    if (!capture(engine,"double-back") || !a || !b || !empty) return false;
    const float fraction = (a->x-empty->x)/(b->x-empty->x);
    std::println("sided.single_delta={} sided.double_delta={} sided.single_fraction={} sided.limit=0.5 sided.minimum=0.005",a->x-empty->x,b->x-empty->x,fraction);
    // Aim at the back of the card explicitly; its absence from the receiver framing cannot establish main-view sidedness.
    render.setCamera(Camera::lookAt(Position{.value=Vec3{0,1.5F,3}},Position{.value=Vec3{-2,1.5F,0}}));
    if (!render.setObjectVisible(*both,false) || !render.setObjectVisible(*single,true) || !gi(engine,false)) return false;
    const auto singleMain=test::linearPoint(render.forward(),Vec3{-2,1.5F,0},"single-main-back");
    if (!render.setObjectVisible(*single,false) || !render.setObjectVisible(*both,true) || !test::renderFrames(engine,4)) return false;
    const auto doubleMain=test::linearPoint(render.forward(),Vec3{-2,1.5F,0},"double-main-back");
    if (!singleMain || !doubleMain || !capture(engine,"double-main-back")) return false;
    std::println("sided.main_single={} sided.main_double={} sided.main.minimum=0.1",singleMain->x,doubleMain->x);
    return b->x > a->x+.005F && fraction < .5F && doubleMain->x > singleMain->x+.1F;
}
/// @brief Compare each metal fraction at the identical receiver location, avoiding spatial irradiance as a confounder.
bool metals(simple::Engine &engine, RenderObjectHandle plane) {
    auto &render = engine.renderSystem();
    if (!materialQuad(engine,directory,"metal-wall",wallPoints,red) || !render.setMaterialLighting(MaterialLighting::physically_based)) return false;
    // Nonzero authored fallback also catches erroneous removal of the old metallic ambient share.
    render.setPointLight(Position{},LinearColor{},LightIntensity{0},LightRange{1},LinearColor{.value=Vec3{.12F}});
    std::array<Vec3,3> deltas{};
    for (auto i : std::views::iota(0U,3U)) {
        const float metal = float(i)*.5F;
        if (!render.setObjectSurface(plane,SurfaceMaterial{.roughness=1,.metalness=metal}) || !gi(engine,false)) return false;
        const auto off = receiver(engine,std::format("metal-{}-off",metal));
        if (!gi(engine,true)) return false;
        const auto on = receiver(engine,std::format("metal-{}-on",metal));
        if (!off || !on) return false;
        deltas[i] = math::subtract(*on,*off);
    }
    const float error = test::rgbDelta(deltas[1],math::scale(deltas[0],.5F));
    std::println("metal.dielectric_delta={} metal.half_delta={} metal.full_delta={} metal.full.limit=0.002 metal.half.error={} metal.half.limit=0.002",deltas[0].x,deltas[1].x,deltas[2].x,error);
    // A raised small dielectric beside the fully metallic reference also verifies the two responses in the same frame.
    const auto beside=render.addPlane(Vec2{.2F,.2F},LinearColor{.value=Vec3{.5F}},Transform{.translation=Position{.value=Vec3{.65F,.01F,0}}});
    if (!beside || !gi(engine,false)) return false;
    const auto sideOff=test::linearPoint(render.forward(),Vec3{.65F,.01F,0},"dielectric-beside-off");
    const auto metalOff=receiver(engine,"metal-beside-off");
    if (!gi(engine,true)) return false;
    const auto sideOn=test::linearPoint(render.forward(),Vec3{.65F,.01F,0},"dielectric-beside-on");
    const auto metalOn=receiver(engine,"metal-beside-on");
    if (!sideOff || !sideOn || !metalOff || !metalOn || !capture(engine,"metal-beside")) return false;
    const float sideDelta=sideOn->x-sideOff->x, metalDelta=test::rgbDelta(*metalOn,*metalOff);
    std::println("metal.beside_dielectric_delta={} metal.beside_metal_delta={} metal.beside.minimum=0.005 metal.beside.maximum=0.002",sideDelta,metalDelta);
    return deltas[0].x > .005F && test::rgbDelta(deltas[2],Vec3{}) < .002F && error < .002F && sideDelta>.005F && metalDelta<.002F;
}
/// @brief Strength interpolates from old ambient to probe light, without scaling direct sun or pure emission.
bool strength(simple::Engine &engine) {
    auto &render = engine.renderSystem();
    // This fixture's sun reference is outside near coverage; retain that fallback premise with the internal coarse baseline.
    render.forward().debugProbeCoarse = false;
    if (!materialQuad(engine,directory,"strength-wall",wallPoints,red)) return false;
    auto camera = Camera::lookAt(Position{.value=Vec3{0,1.5F,3}},Position{}); camera.fov_y.radians=1.5F; render.setCamera(camera);
    render.setDirectionalLight(Direction{.value=Vec3{-1,-1,-1}},LinearColor{},LightIntensity{1},LinearColor{.value=Vec3{.06F}});
    if (!render.addCuboid(Vec3{2.4F,.2F,-.3F},Vec3{2.9F,.8F,.3F},LinearColor{.value=Vec3{.5F}}) ||
        !materialQuad(engine,directory,"emitter",std::array{Vec3{.8F,.3F,.5F},Vec3{1.2F,.3F,.5F},Vec3{1.2F,.9F,.5F},Vec3{.8F,.9F,.5F}},
        R"({"pbrMetallicRoughness":{"baseColorFactor":[0,0,0,1],"metallicFactor":1,"roughnessFactor":1},"emissiveFactor":[0.3,0.7,0.1]})")) return false;
    std::array<Vec3,3> values{}, sun{}, emission{};
    for (auto i : std::views::iota(0U,3U)) {
        if (!gi(engine,i!=0,float(i)*.5F)) return false;
        const auto v=receiver(engine,std::format("strength-{}",float(i)*.5F));
        const auto s=test::linearPoint(render.forward(),Vec3{2.65F,.5F,.301F},"open-sun-outside-volume");
        const auto e=test::linearPoint(render.forward(),Vec3{1,.6F,.5F},"pure-emission");
        if (!v || !s || !e) return false;
        values[i]=*v; sun[i]=*s; emission[i]=*e;
    }
    const auto full=math::subtract(values[2],values[0]), half=math::subtract(values[1],values[0]);
    const float error=test::rgbDelta(half,math::scale(full,.5F));
    const float sunError=test::rgbDelta(sun[0],sun[2]), emissionError=test::rgbDelta(emission[0],emission[2]);
    std::println("strength.full_delta={} strength.half_delta={} strength.error={} strength.limit=0.001 sun.delta={} emissive.delta={} reference.limit=0.0005",full.x,half.x,error,sunError,emissionError);
    return test::rgbDelta(full,Vec3{}) > .005F && error < .001F && sunError < .0005F && emissionError < .0005F && sun[0].x > .1F && emission[0].y > .6F;
}
/// @brief A uniform 8-bit sRGB map and its exact decoded linear factor must transport the same colored energy.
bool srgb(simple::Engine &engine) {
    auto &render=engine.renderSystem();
    auto camera=Camera::lookAt(Position{.value=Vec3{0,1.5F,3}},Position{}); camera.fov_y.radians=1.5F; render.setCamera(camera);
    (void)test::writeFixture(directory,"mask.tga",std::string{"P6\n1 1\n255\n"}+std::string{"\x80\x20\x20",3});
    const auto decode=[](float byte) { const float x=byte/255; return x<=.04045F ? x/12.92F : std::pow((x+.055F)/1.055F,2.4F); };
    const auto material=std::format(R"({{"pbrMetallicRoughness":{{"baseColorFactor":[{},{},{},1],"metallicFactor":0,"roughnessFactor":1}}}})",decode(128),decode(32),decode(32));
    const auto factor=materialQuad(engine,directory,"linear",wallPoints,material);
    const auto texture=materialQuad(engine,directory,"srgb",wallPoints,
        R"({"pbrMetallicRoughness":{"baseColorTexture":{"index":0},"metallicFactor":0,"roughnessFactor":1}})");
    if (!factor || !texture || !render.setObjectVisible(*texture,false) || !render.setObjectVisible(*factor,false) || !gi(engine,true)) return false;
    const auto empty=receiver(engine,"srgb-no-wall");
    if (!render.setObjectVisible(*factor,true) || !test::renderFrames(engine)) return false;
    const auto a=receiver(engine,"linear-factor");
    const auto wallA=test::linearPoint(render.forward(),Vec3{-2,1,0},"linear-wall-main");
    if (!render.setObjectVisible(*factor,false) || !render.setObjectVisible(*texture,true) || !test::renderFrames(engine)) return false;
    const auto b=receiver(engine,"srgb-texture");
    const auto wallB=test::linearPoint(render.forward(),Vec3{-2,1,0},"srgb-wall-main");
    if (!a || !b || !empty || !wallA || !wallB) return false;
    const float error=test::rgbDelta(*a,*b), mainError=test::rgbDelta(*wallA,*wallB);
    std::println("srgb.linear_factor={},{},{} srgb.bounce_error={} srgb.limit=0.001 srgb.main_error={} srgb.main.limit=0.003",decode(128),decode(32),decode(32),error,mainError);
    return error < .001F && mainError < .003F && a->x > empty->x+.003F;
}
} // namespace
/// @brief Each registered subcommand runs on a real Vulkan device and checks validation through teardown.
int main(int argc,char **argv) {
    using namespace vve;
    const std::string_view mode=argc==2 ? argv[1] : "cutout";
    const std::array modes{"cutout","cutout-msaa4","double-sided","metal","strength","srgb"};
    if (argc>2 || std::ranges::find(modes,mode)==modes.end()) return 3;
    directory/=mode; std::filesystem::create_directories(directory); maskTexture();
    auto engine=test::hiddenEngine("gi-material",PixelExtent{128,128}); auto &render=engine.renderSystem();
    if (!engine.init() || !render.setRendererBackend(RendererBackend::hybrid_gi) || !render.setAnimationTime(AnimationTime{}) ||
        !render.setAntialiasing(mode=="cutout-msaa4" ? Antialiasing::msaa4 : Antialiasing::none)) return 1;
    auto camera=Camera::lookAt(Position{.value=Vec3{0,1.5F,3}},Position{}); camera.fov_y.radians=1.1F; render.setCamera(camera);
    render.setBackgroundColor(LinearColor{.value=Vec3{0}});
    render.setDirectionalLight(Direction{.value=Vec3{-1,0,0}},LinearColor{},LightIntensity{4},LinearColor{.value=Vec3{0}});
    render.setPointLight(Position{},LinearColor{},LightIntensity{0},LightRange{1},LinearColor{.value=Vec3{0}});
    const auto plane=render.addPlane(Vec2{1,1},LinearColor{.value=Vec3{.5F}}); if (!plane) return 1;
    bool pass=mode=="double-sided" ? sided(engine) : mode=="metal" ? metals(engine,*plane) : mode=="strength" ? strength(engine) : mode=="srgb" ? srgb(engine) : cutout(engine);
    auto &renderer=render.forward(); VkPhysicalDeviceProperties device{}; vkGetPhysicalDeviceProperties(renderer.physicalDevice.physicalDevice,&device);
    std::println("device={} samples={} mode={} validation_active={} gi.near.gpu_bytes={}",std::string_view{device.deviceName},unsigned(renderer.sampleCount()),mode,renderer.validationActive(),renderer.targets.front().probes.gpuBytes);
#ifndef NDEBUG
    pass &= renderer.validationActive();
#endif
    pass &= renderer.targets.front().probes.gpuBytes==test::probeStorageBytes(renderer);
    render.waitIdle(); engine.gui().shutdownVulkan(); render.shutdown();
    std::println("validation.errors={} test.pass={}",renderer.validationErrorCount(),pass);
    return pass && renderer.validationErrorCount()==0 ? 0 : 2;
}
