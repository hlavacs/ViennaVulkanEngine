export module VEEngine.V3.Systems;
import VEEngine.V3.Types;
import VEEngine;
import std;

export namespace vve::v3 {

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
};

class ISceneSystem : public vve::System {
public:
    [[nodiscard]] virtual std::expected<SceneData, vve::Result> instantiate(
        const ImportedScene& scene) = 0;
};

class ITaskGraphSystem : public vve::System {
public:
    [[nodiscard]] virtual TaskGraph build(
        const SceneData& scene,
        const FrameContext& frame_context) = 0;
};

class IGraphicsBackend {
public:
    virtual ~IGraphicsBackend() = default;
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;
    [[nodiscard]] virtual vve::GraphicsApi api() const noexcept = 0;
    [[nodiscard]] virtual std::expected<void, vve::Result> init() = 0;
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
    [[nodiscard]] virtual RenderGraph build(
        const FrameContext& frame_context,
        const SceneData& scene,
        const TaskGraph& task_graph,
        const ShaderMetadata& shader_metadata) = 0;
    [[nodiscard]] virtual std::expected<void, vve::Result> render(
        const FrameContext& frame_context,
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
};

} // namespace vve::v3
