module VEEngine.V3;
import std;
#include "V3Internal.hpp"

namespace vve::v3::detail {

vve::Handle makeStableHandle(std::string_view name, std::uint64_t salt) {
    const auto mixed = std::format("{}:{}", name, salt);
    return vve::Handle::fromHash(mixed);
}

std::expected<Runtime, vve::Result> createRuntime(const EngineRuntimeDesc& desc) {
    auto graphics_backend = createGraphicsBackend(desc.graphics_api);
    if (!graphics_backend) {
        return std::unexpected(graphics_backend.error());
    }

    Runtime runtime{};
    runtime.asset_system = createAssetSystem();
    runtime.resource_system = createResourceSystem();
    runtime.scene_system = createSceneSystem();
    runtime.task_graph_system = createTaskGraphSystem();
    runtime.graphics_backend = std::move(*graphics_backend);
    runtime.shader_system = createShaderSystem();
    runtime.render_system = createRenderSystem(
        desc.renderer,
        desc.shadow,
        *runtime.graphics_backend,
        desc.imgui_enabled);
    if (desc.imgui_enabled) {
        runtime.gui_system = createGuiSystem();
    }

    runtime.snapshot.graphics_api = desc.graphics_api;
    runtime.snapshot.renderer = desc.renderer;
    runtime.snapshot.shadow = desc.shadow;
    runtime.snapshot.imgui_enabled = desc.imgui_enabled;
    runtime.snapshot.asset_system = std::string(runtime.asset_system->name());
    runtime.snapshot.resource_system = std::string(runtime.resource_system->name());
    runtime.snapshot.scene_system = std::string(runtime.scene_system->name());
    runtime.snapshot.task_graph_system = std::string(runtime.task_graph_system->name());
    runtime.snapshot.shader_system = std::string(runtime.shader_system->name());
    runtime.snapshot.render_system = std::string(runtime.render_system->name());
    runtime.snapshot.gui_system = runtime.gui_system
        ? std::string(runtime.gui_system->name())
        : "Disabled";

    return runtime;
}

} // namespace vve::v3::detail
