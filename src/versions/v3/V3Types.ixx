export module VEEngine.V3.Types;
import VEEngine;
import std;

export namespace vve::v3 {

enum class ResourceKind {
    unknown,
    mesh,
    texture,
    material,
    shader_program,
    buffer,
    image
};

enum class ResourceLocation {
    unknown,
    source_file,
    imported_blob,
    cpu_memory,
    gpu_memory,
    streaming
};

enum class TaskKernelId : std::uint32_t {
    none = 0,
    begin_frame,
    update_transforms,
    sample_animations,
    cull_visibility,
    build_draw_packets,
    upload_resources,
    record_render_graph,
    record_post_processing,
    record_post_post_processing
};

enum class RenderKernelId : std::uint32_t {
    none = 0,
    depth_prepass,
    forward_opaque,
    deferred_gbuffer,
    deferred_lighting,
    path_trace,
    shadow_map,
    ray_traced_shadows,
    post_process,
    post_post_process,
    imgui
};

enum class RenderTaskPhase : std::uint32_t {
    main = 0,
    post_process,
    post_post_process
};

enum class ShaderStage : std::uint32_t {
    vertex = 0,
    fragment,
    compute
};

struct MeshHandle final {
    vve::Handle value{};
};

struct TextureHandle final {
    vve::Handle value{};
};

struct MaterialHandle final {
    vve::Handle value{};
};

struct ShaderHandle final {
    vve::Handle value{};
};

struct SceneHandle final {
    vve::Handle value{};
};

struct SceneNodeHandle final {
    vve::Handle value{};
};

struct TaskNodeHandle final {
    vve::Handle value{};
};

struct RenderPassHandle final {
    vve::Handle value{};
};

struct FrameContext {
    std::uint64_t frame_index{0};
    double delta_seconds{0.0};
};

struct ResourceAccess {
    vve::Handle resource{};
    bool write{false};
};

struct ImportedMesh {
    MeshHandle handle{};
    std::string name{};
};

struct ImportedMaterial {
    MaterialHandle handle{};
    std::string name{};
};

struct ImportedSceneNode {
    SceneNodeHandle handle{};
    SceneNodeHandle parent{};
    std::string name{};
    vve::math::Mat4 local_transform{vve::math::identityMat4()};
};

struct ImportedScene {
    SceneHandle handle{};
    std::string name{};
    std::vector<ImportedMesh> meshes{};
    std::vector<ImportedMaterial> materials{};
    std::vector<ImportedSceneNode> nodes{};
};

struct ResourceRecord {
    vve::Handle id{};
    ResourceKind kind{ResourceKind::unknown};
    ResourceLocation location{ResourceLocation::unknown};
    std::uint32_t generation{0};
    std::filesystem::path source_path{};
};

struct SceneNodeDesc {
    SceneNodeHandle handle{};
    SceneNodeHandle parent{};
    std::string name{};
    vve::math::Mat4 local_transform{vve::math::identityMat4()};
};

struct SceneData {
    SceneHandle handle{};
    std::vector<SceneNodeDesc> nodes{};
};

struct TaskExecutionContext {
    const FrameContext* frame_context{nullptr};
    const SceneData* scene{nullptr};
};

using TaskCallback = std::function<std::expected<void, vve::Result>(const TaskExecutionContext&)>;

struct TaskNodeDesc {
    TaskNodeHandle handle{};
    TaskKernelId kernel{TaskKernelId::none};
    std::vector<TaskNodeHandle> depends_on{};
    std::vector<ResourceAccess> accesses{};
    std::string debug_name{};
    TaskCallback callback{};
};

struct TaskGraph {
    std::vector<TaskNodeDesc> nodes{};
};

class TaskGraphBuilder {
public:
    [[nodiscard]] TaskNodeHandle addTask(
        std::string_view stable_name,
        TaskKernelId kernel,
        TaskCallback callback = {},
        std::vector<TaskNodeHandle> depends_on = {},
        std::vector<ResourceAccess> accesses = {},
        std::string debug_name = {});

    void addTask(TaskNodeDesc node);

    [[nodiscard]] TaskGraph build() &&;
    [[nodiscard]] std::vector<TaskNodeHandle> leafTasks() const;

private:
    std::vector<TaskNodeDesc> nodes_{};
};

inline TaskNodeHandle TaskGraphBuilder::addTask(
    std::string_view stable_name,
    TaskKernelId kernel,
    TaskCallback callback,
    std::vector<TaskNodeHandle> depends_on,
    std::vector<ResourceAccess> accesses,
    std::string debug_name) {
    const TaskNodeHandle handle{vve::Handle::fromHash(stable_name)};
    addTask(TaskNodeDesc{
        .handle = handle,
        .kernel = kernel,
        .depends_on = std::move(depends_on),
        .accesses = std::move(accesses),
        .debug_name = debug_name.empty() ? std::string(stable_name) : std::move(debug_name),
        .callback = std::move(callback)
    });
    return handle;
}

inline void TaskGraphBuilder::addTask(TaskNodeDesc node) {
    if (node.debug_name.empty()) {
        node.debug_name = "task." + std::to_string(node.handle.value.value());
    }

    nodes_.push_back(std::move(node));
}

inline TaskGraph TaskGraphBuilder::build() && {
    return TaskGraph{.nodes = std::move(nodes_)};
}

inline std::vector<TaskNodeHandle> TaskGraphBuilder::leafTasks() const {
    std::unordered_set<vve::Handle::value_type> dependency_handles{};
    dependency_handles.reserve(nodes_.size());

    for (const auto& node : nodes_) {
        for (const auto& dependency : node.depends_on) {
            dependency_handles.insert(dependency.value.value());
        }
    }

    std::vector<TaskNodeHandle> leaf_handles{};
    leaf_handles.reserve(nodes_.size());
    for (const auto& node : nodes_) {
        if (!dependency_handles.contains(node.handle.value.value())) {
            leaf_handles.push_back(node.handle);
        }
    }

    return leaf_handles;
}

struct ShaderParameter {
    std::string name{};
    std::string type_name{};
    std::uint32_t binding{0};
    std::uint32_t set{0};
};

struct ShaderMetadata {
    ShaderHandle handle{};
    std::string shader_name{};
    std::vector<ShaderStage> stages{};
    std::vector<ShaderParameter> parameters{};
    std::string intended_renderer{};
    std::string intended_shadow{};
};

struct RenderResourceUse {
    vve::Handle resource{};
    bool write{false};
};

struct RenderPassDesc {
    RenderPassHandle handle{};
    RenderKernelId kernel{RenderKernelId::none};
    RenderTaskPhase phase{RenderTaskPhase::main};
    std::vector<RenderPassHandle> depends_on{};
    std::vector<RenderResourceUse> uses{};
    std::string debug_name{};
};

struct RenderGraph {
    std::vector<RenderPassDesc> passes{};
};

struct EngineRuntimeSnapshot {
    vve::GraphicsApi graphics_api{vve::GraphicsApi::vulkan};
    vve::RendererKind renderer{vve::RendererKind::forward_renderer};
    vve::ShadowKind shadow{vve::ShadowKind::none};
    bool imgui_enabled{true};
    std::string asset_system{"AssimpAssetSystem"};
    std::string resource_system{"ResourceSystem"};
    std::string scene_system{"SceneSystem"};
    std::string task_graph_system{"TaskGraphSystem"};
    std::string shader_system{"SlangShaderSystem"};
    std::string render_system{"RenderSystem"};
    std::string gui_system{"ImGuiSystem"};
    std::vector<std::string> task_systems{};
};

} // namespace vve::v3
