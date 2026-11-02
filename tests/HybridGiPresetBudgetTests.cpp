/** @file @brief Deterministic 1080p low-preset allocation and capture-budget contracts through the public facade. */
import std;
import VVEngine;

/// @brief Checks actual allocations and recorded work at both coverage counts; no wall-clock assertions or pixel readbacks.
int main() {
    using namespace vve;
    constexpr std::uint64_t memoryLimit = 12U * 1024U * 1024U; // Documented single-window ceiling, including full-resolution AO.
    for (const auto aa : {Antialiasing::none,Antialiasing::msaa4}) {
        Engine<> engine{ApplicationName{"gi-preset-budget"},WindowSetups{WindowSetup{}.extent({1920,1080}).visible(false)}};
        auto world = engine.world(); auto &render = world.get<RenderSystem>();
        if (!engine.init() || !render.setAntialiasing(aa) || !render.setRendererBackend(RendererBackend::hybrid_gi) ||
            !render.setGiSettings({.quality = GiQuality::low})) return 1;
        const auto window = world.get<WindowSystem>().findWindow("main");
        if (!window) return 1;
        std::uint64_t allocated{};
        // AO toggles retain storage; the low preset must never exceed one complete cubemap capture in a submitted frame.
        for (const bool ao : {false,true,false}) {
            if (!render.setGiSettings({.ao_enabled = ao,.quality = GiQuality::low})) return 1;
            for ([[maybe_unused]] const auto frame : std::views::iota(0U,4U)) {
                if (!engine.step()) return 1;
                const auto receipt = render.renderDiagnostics(window->handle(),false);
                if (!receipt) return 1;
                std::uint64_t bytes{}, updated{};
                for (const auto &[key,value] : *receipt) {
                    if (key == "gi.gpu_bytes") bytes = std::stoull(value);
                    if (key == "gi.probes.updated") updated = std::stoull(value);
                }
                if (!bytes || bytes > memoryLimit || updated != 1 || (allocated && bytes != allocated)) return 2;
                allocated = bytes;
                std::println("preset=low samples={} ao.enabled={} gi.gpu_bytes={} gi.bytes.limit={} gi.probes.updated={} gi.update.limit=1",
                    aa == Antialiasing::msaa4 ? 4 : 1,ao,bytes,memoryLimit,updated);
            }
        }
#ifndef NDEBUG
        if (!render.validationActive()) return 2;
#endif
        // Engine teardown owns completion; CTest also rejects validation diagnostics emitted during destruction.
        std::println("validation.active={} validation.errors={}",render.validationActive(),render.validationErrorCount());
        if (render.validationErrorCount()) return 2;
    }
    return 0;
}
