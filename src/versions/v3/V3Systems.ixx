export module VEEngine.V3.Systems;
import VEEngine.V3.Types;
import VEEngine;
import std;

export namespace vve::v3 {

class IGraphicsBackend;
class IResourceSystem;
class ISceneSystem;
class IRenderSystem;

class IAssetSystem : public vve::System {
public:
    [[nodiscard]] virtual std::expected<ImportedScene, vve::Result> importScene(
        const std::filesystem::path& source_path) = 0;
};

class IResourceSystem : public vve::System {
public:
    [[nodiscard]] virtual std::expected<void, vve::Result> registerImportedScene(
        const ImportedScene& scene,
        const std::filesystem::path& source_path) = 0;
    [[nodiscard]] virtual std::expected<std::vector<ResourceRecord>, vve::Result> enumerate() const = 0;
    [[nodiscard]] virtual std::expected<void, vve::Result> uploadResources(
        const FrameContext& frame_context,
        const SceneData& scene) = 0;
    virtual void registerTasks(
        TaskGraphBuilder& builder,
        const SceneData& scene) = 0;
};

class ISceneSystem : public vve::System {
public:
    [[nodiscard]] virtual std::expected<SceneData, vve::Result> instantiate(
        const ImportedScene& scene) = 0;
    [[nodiscard]] virtual std::expected<void, vve::Result> updateTransforms(
        const FrameContext& frame_context,
        SceneData& scene) = 0;
    [[nodiscard]] virtual std::expected<void, vve::Result> cullVisibility(
        const FrameContext& frame_context,
        const SceneData& scene) = 0;
    virtual void registerTasks(
        TaskGraphBuilder& builder,
        const SceneData& scene) = 0;
};

class ITaskSystem : public vve::System {
public:
    virtual void registerTasks(
        TaskGraphBuilder& builder,
        const SceneData& scene) = 0;
};

class ITaskGraphSystem : public vve::System {
public:
    [[nodiscard]] virtual TaskGraph build(
        const SceneData& scene,
        std::span<ITaskSystem* const> task_systems,
        IGraphicsBackend& graphics_backend,
        IResourceSystem& resource_system,
        ISceneSystem& scene_system,
        IRenderSystem& render_system,
        const RenderGraph& render_graph) = 0;
};

class IGraphicsBackend {
public:
    virtual ~IGraphicsBackend() = default;
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;
    [[nodiscard]] virtual vve::GraphicsApi api() const noexcept = 0;
    [[nodiscard]] virtual std::expected<void, vve::Result> init() = 0;
    [[nodiscard]] virtual std::expected<void, vve::Result> beginFrame(
        const FrameContext& frame_context) = 0;
    [[nodiscard]] virtual std::expected<void, vve::Result> endFrame(
        const FrameContext& frame_context) = 0;
    virtual void registerTasks(TaskGraphBuilder& builder) = 0;
};

class IShaderSystem : public vve::System {
public:
    [[nodiscard]] virtual std::expected<ShaderMetadata, vve::Result> reflect(
        const std::filesystem::path& shader_path,
        vve::RendererKind renderer,
        vve::ShadowKind shadow) = 0;
};

class IRenderSystem : public vve::System {
public:
    [[nodiscard]] virtual RenderGraph buildStaticGraph() = 0;
    [[nodiscard]] virtual std::expected<void, vve::Result> cullVisibilityGpu(
        const FrameContext& frame_context,
        const SceneData& scene,
        const RenderGraph& render_graph) = 0;
    [[nodiscard]] virtual std::expected<void, vve::Result> buildDrawPackets(
        const FrameContext& frame_context,
        const SceneData& scene,
        const RenderGraph& render_graph) = 0;
    [[nodiscard]] virtual std::expected<void, vve::Result> record(
        const FrameContext& frame_context,
        const SceneData& scene,
        const RenderGraph& render_graph) = 0;
    [[nodiscard]] virtual std::expected<void, vve::Result> consumeOutput(
        const FrameContext& frame_context,
        const SceneData& scene,
        const RenderGraph& render_graph) = 0;
    virtual void registerTasks(
        TaskGraphBuilder& builder,
        const SceneData& scene,
        const RenderGraph& render_graph) = 0;
};

class IGuiSystem : public vve::System {
public:
    [[nodiscard]] virtual std::expected<void, vve::Result> init(
        IGraphicsBackend& graphics_backend) = 0;
};

struct EngineRuntimeDesc {
    vve::GraphicsApi graphics_api{vve::GraphicsApi::vulkan};
    vve::RendererKind renderer{vve::RendererKind::forward_renderer};
    vve::ShadowKind shadow{vve::ShadowKind::none};
    bool imgui_enabled{true};
    std::vector<std::shared_ptr<ITaskSystem>> task_systems{};
};

struct TaskSystems {
    std::vector<std::shared_ptr<ITaskSystem>> value{};
};

} // namespace vve::v3
