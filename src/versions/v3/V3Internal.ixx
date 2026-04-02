export module VEEngine.V3:Internal;
import VEEngine.V3.Types;
import VEEngine.V3.Systems;
import VEEngine;
import std;

export namespace vve::v3::detail {

struct Runtime final {
    std::unique_ptr<IAssetSystem> asset_system{};
    std::unique_ptr<IResourceSystem> resource_system{};
    std::unique_ptr<ISceneSystem> scene_system{};
    std::unique_ptr<ITaskGraphSystem> task_graph_system{};
    std::vector<std::shared_ptr<ITaskSystem>> task_systems{};
    std::unique_ptr<IWindowSystem> window_system{};
    std::shared_ptr<WindowFrameData> window_frame{std::make_shared<WindowFrameData>()};
    std::unique_ptr<IGraphicsBackend> graphics_backend{};
    std::unique_ptr<IShaderSystem> shader_system{};
    std::unique_ptr<IRenderSystem> render_system{};
    std::vector<WindowRenderPipeline> render_pipelines{};
    std::unique_ptr<IGuiSystem> gui_system{};
    EngineRuntimeSnapshot snapshot{};
};

[[nodiscard]] vve::Handle makeStableHandle(std::string_view name, std::uint64_t salt = 0);

[[nodiscard]] std::unique_ptr<IAssetSystem> createAssetSystem();
[[nodiscard]] std::unique_ptr<IResourceSystem> createResourceSystem();
[[nodiscard]] std::unique_ptr<ISceneSystem> createSceneSystem();
[[nodiscard]] std::unique_ptr<ITaskGraphSystem> createTaskGraphSystem();
[[nodiscard]] std::unique_ptr<IWindowSystem> createWindowSystem();
[[nodiscard]] std::expected<std::unique_ptr<IGraphicsBackend>, vve::Error> createGraphicsBackend(
    vve::GraphicsApi api);
[[nodiscard]] std::unique_ptr<IShaderSystem> createShaderSystem();
[[nodiscard]] std::unique_ptr<IRenderSystem> createRenderSystem(
    vve::RendererKind renderer,
    vve::ShadowKind shadow,
    IGraphicsBackend& graphics_backend,
    bool imgui_enabled);
[[nodiscard]] std::unique_ptr<IGuiSystem> createGuiSystem();
[[nodiscard]] std::expected<Runtime, vve::Error> createRuntime(const EngineRuntimeDesc& desc);

} // namespace vve::v3::detail
