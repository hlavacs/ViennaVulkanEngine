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
    update_transforms,
    sample_animations,
    cull_visibility,
    build_draw_packets,
    upload_resources
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
    imgui
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

struct TaskNodeDesc {
    TaskNodeHandle handle{};
    TaskKernelId kernel{TaskKernelId::none};
    std::vector<TaskNodeHandle> depends_on{};
    std::vector<ResourceAccess> accesses{};
};

struct TaskGraph {
    std::vector<TaskNodeDesc> nodes{};
};

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
};

} // namespace vve::v3
