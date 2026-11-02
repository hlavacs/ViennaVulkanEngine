/** @file @brief Public transactional GI controls, independent settings, real-device runtime changes and both budgets. */
#include <vulkan/vulkan_core.h>
import std;
import VVEngine;
import VVEngine.Simple;
import VVEngine.Simple.Renderer;
import VVEngine.Simple.IrradianceProbes;
import VVE.TestSupport;

/// @brief Malformed requests must preserve every field; AO is stored without allocating or running an AO pass.
int main() {
    using namespace vve;
    Engine<> facade;
    auto world = facade.world(); auto &publicRender = world.get<RenderSystem>();
    const GiSettings selected{.enabled = false,.ao_enabled = true,.indirect_strength = .75F,.quality = GiQuality::low};
    if (!publicRender.setGiSettings(selected)) return 1;
    const auto unchanged = [&] {
        const auto s = publicRender.giSettings();
        return s.enabled == selected.enabled && s.ao_enabled == selected.ao_enabled &&
            s.indirect_strength == selected.indirect_strength && s.quality == selected.quality;
    };
    for (const auto strength : {-1.0F,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}) {
        auto bad = selected; bad.enabled = true; bad.indirect_strength = strength;
        const auto result = publicRender.setGiSettings(bad);
        if (result || result.error() != Error::invalid_argument || !unchanged()) return 2;
    }
    auto bad = selected; bad.quality = static_cast<GiQuality>(99);
    const auto result = publicRender.setGiSettings(bad);
    if (result || result.error() != Error::invalid_argument || !unchanged()) return 2;
    auto engine = test::hiddenEngine("gi-settings",PixelExtent{64,64}); auto &render = engine.renderSystem();
    if (!render.giSettings().enabled || !engine.init() || !render.setRendererBackend(RendererBackend::hybrid_gi)) return 1;
    // Keep the fallback reference above both volumes (coarse top y=8), preserving the exact equality premise.
    render.setCamera(Camera::lookAt(Position{.value = Vec3{0,0,3}},Position{.value = Vec3{0,12,0}}));
    if (!render.addCuboid(Vec3{-.5F},Vec3{.5F},LinearColor{.value = Vec3{.5F}},Transform{.translation = Position{.value = Vec3{0,12,0}}}) ||
        !render.setGiSettings(GiSettings{.enabled = false}) || !engine.renderFrame()) return 1;
    const auto off = test::linearRegion(render.forward(),30,30,4,4);
    if (!off || !render.setGiSettings(GiSettings{.quality = GiQuality::low}) || !engine.renderFrame()) return 1;
    auto &renderer = render.forward(); auto &p = renderer.targets.front().probes;
    if (p.updated != 1 || p.valid != 1 || p.gpuBytes == 0) return 2;
    const auto bytes = p.gpuBytes;
    // Quality replacement starts fresh storage; old views retire by submission fences.
    const auto resets = renderer.targets.front().probeResets;
    if (!render.setGiSettings(GiSettings{}) || !engine.renderFrame() || p.updated != 2 || p.valid != 2 ||
        renderer.targets.front().probeResets != resets + 1) return 2;
    VkPhysicalDeviceProperties device{}; vkGetPhysicalDeviceProperties(renderer.physicalDevice.physicalDevice,&device);
    const auto color = test::linearRegion(renderer,30,30,4,4);
    if (!color || std::max({std::abs(color->x-off->x),std::abs(color->y-off->y),std::abs(color->z-off->z)}) > .00001F) return 2;
    std::println("device={} gi.probes.valid={} gi.probes.updated={} gi.near.gpu_bytes={} receiver.linear.off={},{},{} receiver.linear.on={},{},{} invalid_preserves=1 ao_stored=1 validation_active={}",
        std::string_view{device.deviceName},p.valid,p.updated,p.gpuBytes,off->x,off->y,off->z,color->x,color->y,color->z,renderer.validationActive());
    // Teleporting beyond the volume clears old coefficients and publishes fallback for the entire first frame.
    render.setCamera(Camera::lookAt(Position{.value = Vec3{20,0,3}},Position{.value = Vec3{20,0,0}}));
    if (!engine.renderFrame() || p.valid != 0 || p.updated != 0 || p.gpuBytes != bytes) return 2;
    std::println("gi.grid.reset_valid={} gi.grid.origin={},{},{} gi.near.gpu_bytes={}",p.valid,p.origin.x,p.origin.y,p.origin.z,p.gpuBytes);
    render.waitIdle(); engine.gui().shutdownVulkan(); render.shutdown();
    std::println("validation.errors={}",renderer.validationErrorCount());
    return renderer.validationErrorCount() == 0 ? 0 : 2;
}
