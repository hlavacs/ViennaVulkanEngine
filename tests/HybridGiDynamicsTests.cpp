/** @file @brief GPU toroidal overlap, priority, stale refresh and fallback-only resets in deterministic HDR fixtures. */
#include <vulkan/vulkan_core.h>
#include "../src/versions/simple/shaders/simple_shared.h"
import std;
import VVEngine;
import VVEngine.Simple;
import VVEngine.Simple.Renderer;
import VVEngine.Simple.IrradianceProbes;
import VVE.TestSupport;

namespace {
using namespace vve;
constexpr unsigned count = VVE_PROBE_COUNT; ///< Fixed volume capacity, shared with both shader indexing paths.
constexpr Vec3 point{0,0,.5F}; ///< Shared fully covered boundary of both volumes' interiors after a one-cell shift.
/// @brief Renders an exact window-frame budget, without changing time or scene data.
bool warm(simple::Engine &engine, unsigned frames) {
    for (auto frame : std::views::iota(0U,frames)) if (!engine.renderFrame()) return false;
    return true;
}
/// @brief Maximum HDR channel error prevents color transport from hiding in a whole-frame average.
float difference(Vec3 a, Vec3 b) { return std::max({std::abs(a.x-b.x),std::abs(a.y-b.y),std::abs(a.z-b.z)}); }
/// @brief Projects a fixed world receiver point; checks a bounded HDR patch for finite, nonnegative radiance.
std::optional<Vec3> measure(simple::Engine &engine, Camera camera, std::string_view state = {}, Vec3 offset = {}) {
    const auto receiver = math::add(point,offset);
    const auto clip = math::multiply(math::multiply(math::perspectiveVulkan(camera.fov_y.radians,1,.1F,100),
        math::lookAt(camera.position.value,math::add(camera.position.value,camera.forward.value),Vec3{0,1,0})),Vec4{receiver,1});
    const auto x = static_cast<unsigned>((clip.x / clip.w * .5F + .5F) * 128) - 2;
    const auto y = static_cast<unsigned>((clip.y / clip.w * .5F + .5F) * 128) - 2;
    const auto value = test::linearRegion(engine.renderSystem().forward(),x,y,4,4);
    if (!value || !std::isfinite(value->x) || !std::isfinite(value->y) || !std::isfinite(value->z) ||
        std::min({value->x,value->y,value->z}) < 0) return {};
    if (!state.empty()) {
        const auto &p = engine.renderSystem().forward().targets.front().probes;
        std::println("state={} receiver.linear={:.7f},{:.7f},{:.7f} gi.probes.valid={} gi.probes.stale={} gi.probes.updated={}",
            state,value->x,value->y,value->z,p.valid,p.stale,p.updated);
    }
    return value;
}
/// @brief Refreshes after an edit without discarding old light, checking finite radiance on every intermediate frame.
std::optional<Vec3> refresh(simple::Engine &engine, Camera camera, unsigned budget, std::string_view event) {
    auto &p = engine.renderSystem().forward().targets.front().probes;
    const auto bound = (count + budget - 1) / budget;
    float minimum = std::numeric_limits<float>::max();
    std::optional<Vec3> value;
    for (auto frame : std::views::iota(1U,bound + 1)) {
        if (!engine.renderFrame() || p.updated != budget || p.valid != count || p.stale != count - frame * budget) return {};
        value = measure(engine,camera,frame == 1 || frame == bound ? event : std::string_view{});
        if (!value || value->x < .001F) return {}; // The dim neutral sky keeps physically unlit receivers above black.
        minimum = std::min(minimum,value->x);
    }
    std::println("event={} convergence.frames={} convergence.bound={} receiver.minimum.red={}",event,bound,bound,minimum);
    return value;
}
/// @brief An independently cleared/warmed volume gives a fresh reference for the exact same scene/camera.
std::optional<Vec3> reference(simple::Engine &engine, Camera camera, unsigned frames) {
    auto &renderer = engine.renderSystem().forward();
    renderer.invalidateProbes(true);
    if (!engine.renderFrame() || renderer.targets.front().probes.valid != 0 || !warm(engine,frames)) return {};
    return measure(engine,camera,"fresh-reference");
}
}

/// @brief Every subcommand runs on the actual selected Vulkan device with validation through teardown.
int main(int argc, char **argv) {
    using namespace vve;
    const std::string_view mode = argc == 1 ? "sun" : std::string_view{argv[1]};
    if (argc > 2 || (mode != "sun" && mode != "sun-low" && mode != "scroll" && mode != "scroll-low" &&
        mode != "sky" && mode != "move" && mode != "teleport" && mode != "replacement")) return 3;
    const bool low = mode.ends_with("-low"), scroll = mode.starts_with("scroll");
    const unsigned budget = low ? 1 : VVE_PROBE_MAX_BUDGET, bound = (count + budget - 1) / budget;
    auto engine = test::hiddenEngine("probe-dynamics",PixelExtent{128,128});
    auto &render = engine.renderSystem();
    if (!engine.init() || !render.setRendererBackend(RendererBackend::hybrid_gi) || !render.setAnimationTime(AnimationTime{}) ||
        !render.setGiSettings(GiSettings{.quality = low ? GiQuality::low : GiQuality::medium})) return 1;
    // A narrow fixed footprint isolates the shared world point from the opposite volume-edge fades after scrolling.
    auto camera = Camera::lookAt(Position{.value = Vec3{0,1.5F,3}},Position{.value = point}, {}, FovY{.25F});
    if (scroll && low) camera = Camera::lookAt(Position{.value = Vec3{VVE_PROBE_SPACING,1.5F,3}},Position{.value = point}, {}, camera.fov_y);
    render.setCamera(camera);
    render.setBackgroundColor(LinearColor{.value = Vec3{.02F}}); // A finite no-wall baseline exposes transient black holes.
    render.setPointLight(Position{},LinearColor{},LightIntensity{0},LightRange{1},LinearColor{.value = Vec3{.02F}});
    const auto sun = [&](float intensity) {
        render.setDirectionalLight(Direction{.value = Vec3{-1,0,0}},LinearColor{},LightIntensity{intensity},LinearColor{.value = Vec3{0}});
    };
    sun(4);
    auto floor = render.addPlane(Vec2{1,1},LinearColor{.value = Vec3{.5F}});
    auto wall = render.addCuboid(Vec3{-2.2F,0,-2},Vec3{-2,3,2},LinearColor{.value = Vec3{1,.01F,.01F}});
    // Populate coarse as well: otherwise its never-captured rows correctly outrank the stale near rows tested below.
    if (!floor || !wall || !warm(engine,bound * VVE_PROBE_CASCADES)) return 1;
    auto &renderer = render.forward(); auto &p = renderer.targets.front().probes;
    const auto before = measure(engine,camera,"converged-red");
    const auto initial = test::probeValidity(renderer);
    if (!before || !initial || initial->first != count || p.stale != 0 || before->x <= before->y + .005F) return 2;
    bool pass = true;
    if (scroll) {
        // Medium crosses +x; low crosses -x, exercising negative modular cells and both exposed slab directions.
        camera.position.value.x += low ? -VVE_PROBE_SPACING : VVE_PROBE_SPACING;
        camera = Camera::lookAt(camera.position,Position{.value = point}, {}, camera.fov_y);
        render.setCamera(camera);
        if (!engine.renderFrame()) return 1;
        const auto shifted = measure(engine,camera,"scroll-first");
        const auto first = test::probeValidity(renderer);
        constexpr unsigned slab = VVE_PROBE_SIDE * VVE_PROBE_SIDE;
        if (!shifted || !first) return 1;
        const auto drift = difference(*before,*shifted);
        pass &= first->first == count - slab + budget && first->second == 0 && p.valid == first->first && drift < .002F;
        const auto slabBound = (slab + budget - 1) / budget;
        // Missing rows must win over every old capture: all 16 exposed cells are captured within the slab budget.
        for (auto frame : std::views::iota(1U,slabBound)) {
            if (!engine.renderFrame() || p.valid != count - slab + (frame + 1) * budget) return 2;
        }
        const auto complete = test::probeValidity(renderer);
        std::println("scroll.overlap.valid={} scroll.first.valid={} scroll.receiver.delta={} scroll.convergence.frames={} scroll.convergence.bound={}",
            count-slab,first->first,drift,slabBound,slabBound);
        pass &= complete && complete->first == count;
    } else if (mode.starts_with("sun") || mode == "sky" || mode == "move") {
        if (mode.starts_with("sun")) sun(1);
        if (mode == "sky") {
            const auto panorama = test::writeFixture(std::filesystem::path{VVE_TEST_TMP_DIR},"blue-sky.ppm",
                std::string{"P6\n2 1\n255\n"} + std::string{"\x60\x9c\xee\x60\x9c\xee",6});
            if (!render.setSkybox(panorama)) return 1;
        }
        if (mode == "move" && !render.setObjectTransform(*wall,Transform{.translation = Position{.value = Vec3{-100,0,0}}})) return 1;
        const auto after = refresh(engine,camera,budget,mode);
        const auto fresh = reference(engine,camera,bound);
        if (!after || !fresh) return 1;
        const auto delta = difference(*after,*fresh);
        pass &= delta < .002F;
        if (mode.starts_with("sun")) pass &= after->x < before->x - .005F && after->x > .001F;
        if (mode == "sky") pass &= after->z > before->z + .005F;
        if (mode == "move") {
            if (!render.removeObject(*wall) || !warm(engine,bound)) return 1;
            const auto noWall = measure(engine,camera,"no-wall");
            if (!noWall) return 1;
            const auto removal = difference(*after,*noWall);
            pass &= before->x > after->x + .005F && removal < .003F;
            std::println("move.no_wall.delta={} limit=0.003",removal);
        }
        std::println("event={} reference.delta={} limit=0.002",mode,delta);
    } else {
        const bool teleport = mode == "teleport";
        const Vec3 offset = teleport ? Vec3{20,0,0} : Vec3{};
        if (teleport) {
            // Translate the fixture too, so the relocated receiver still has exactly the same useful bounce scene.
            constexpr float distance = 20;
            if (!render.setObjectTransform(*floor,Transform{.translation = Position{.value = Vec3{distance,0,0}}}) ||
                !render.setObjectTransform(*wall,Transform{.translation = Position{.value = Vec3{distance,0,0}}})) return 1;
            camera.position.value.x += distance;
            camera = Camera::lookAt(camera.position,Position{.value = math::add(point,offset)}, {}, camera.fov_y);
            render.setCamera(camera);
        } else {
            render.clearScene(); sun(4);
            floor = render.addPlane(Vec2{1,1},LinearColor{.value = Vec3{.5F}});
            wall = render.addCuboid(Vec3{-2.2F,0,-2},Vec3{-2,3,2},LinearColor{.value = Vec3{.01F,1,.01F}});
            if (!floor || !wall) return 1;
        }
        if (!engine.renderFrame()) return 1;
        const auto reset = test::probeValidity(renderer);
        const auto resetValue = measure(engine,camera,"reset",offset);
        pass &= reset && reset->first == 0 && reset->second == 0 && p.valid == 0 && p.updated == 0 && p.stale == 0;
        auto settings = render.giSettings(); settings.enabled = false;
        if (!render.setGiSettings(settings) || !engine.renderFrame()) return 1;
        const auto fallback = measure(engine,camera,"reset-gi-off",offset);
        if (!resetValue || !fallback) return 1;
        const auto delta = difference(*resetValue,*fallback);
        pass &= std::isfinite(resetValue->x) && std::isfinite(resetValue->y) && std::isfinite(resetValue->z) && delta < .002F;
        settings.enabled = true;
        if (!render.setGiSettings(settings) || !warm(engine,bound)) return 1;
        const auto after = measure(engine,camera,"reset-converged",offset);
        const auto complete = test::probeValidity(renderer);
        if (!after) return 1;
        pass &= complete && complete->first == count && p.stale == 0;
        pass &= teleport ? after->x > fallback->x + .005F : after->y > after->x * 1.5F && after->y > fallback->y + .005F;
        if (teleport) pass &= difference(*after,*before) < .002F;
        else {
            const auto fresh = reference(engine,camera,bound);
            if (!fresh) return 1;
            const auto delta = difference(*after,*fresh);
            std::println("replacement.reference.delta={} limit=0.002",delta);
            pass &= delta < .002F; // Green must match a fresh volume, not merely overpower stale red.
        }
        std::println("event={} reset.valid={} reset.fallback.delta={} convergence.frames={} receiver.linear={},{},{}",
            mode,reset ? reset->first : count,delta,bound,after->x,after->y,after->z);
    }
    VkPhysicalDeviceProperties device{}; vkGetPhysicalDeviceProperties(renderer.physicalDevice.physicalDevice,&device);
    const bool validation = renderer.validationActive();
    const auto bytes = p.gpuBytes;
    render.waitIdle(); engine.gui().shutdownVulkan(); render.shutdown();
    std::println("device={} mode={} budget={} gi.near.gpu_bytes={} validation_active={} validation.errors={} pass={}",
        std::string_view{device.deviceName},mode,budget,bytes,validation,renderer.validationErrorCount(),pass);
#ifndef NDEBUG
    pass &= validation;
#endif
    return pass && renderer.validationErrorCount() == 0 ? 0 : 2;
}
