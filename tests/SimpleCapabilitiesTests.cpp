import std;
import VVEngine;

int main() {
    using namespace vve;
    Engine<> engine;
    auto world = engine.world();
    auto &render = world.get<RenderSystem>();
    const auto unsupported = [](const auto &result) {
        return !result && result.error() == Error::unsupported_feature;
    };
    if (render.rendererBackend() != RendererBackend::forward ||
        render.materialLighting() != MaterialLighting::classic) return 1;
    if (!render.setRendererBackend(RendererBackend::deferred) ||
        render.materialLighting() != MaterialLighting::physically_based) return 2;
    if (!unsupported(render.setRendererBackend(RendererBackend::hybrid_gi)) ||
        render.rendererBackend() != RendererBackend::deferred) return 3;
    if (!unsupported(render.setGiSettings({})) || render.giSettings().enabled) return 4;
    if (!unsupported(render.setAntialiasing(Antialiasing::smaa)) ||
        !unsupported(render.setAntialiasing(Antialiasing::msaa4_smaa)) ||
        render.antialiasing() != Antialiasing::none) return 5;
    if (!unsupported(render.setToneMapping({ToneMappingOperator::khronos_neutral})) ||
        render.toneMapping().op != ToneMappingOperator::none) return 6;
    if (!render.setAntialiasing(Antialiasing::msaa4)) return 7;
    for (unsigned i = 0; i < 11; ++i) {
        render.addDirectionalLight(DirectionalLight{});
        render.addPointLight(PointLight{});
        const SpotLight spot{};
        render.addSpotLight(spot.position, spot.direction, spot.color, spot.intensity, spot.range, spot.cone);
    }
    return render.sceneDirectionalLightCount() == 10 && render.scenePointLightCount() == 10 &&
        render.sceneSpotLightCount() == 10 ? 0 : 8;
}
