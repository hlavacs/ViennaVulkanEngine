module VEEngine.V3;
import std;
import :Internal;

namespace vve::v3 {

namespace {

class ResourceSystem final : public IResourceSystem {
public:
    [[nodiscard]] std::string_view name() const noexcept override {
        return "ResourceSystem";
    }

    [[nodiscard]] std::expected<void, vve::Error> registerImportedScene(
        const ImportedScene& scene,
        const std::filesystem::path& source_path) override {
        records_.push_back(ResourceRecord{
            .id = scene.handle.value,
            .kind = ResourceKind::unknown,
            .location = ResourceLocation::source_file,
            .generation = 1,
            .source_path = source_path
        });

        for (const auto& mesh : scene.meshes) {
            records_.push_back(ResourceRecord{
                .id = mesh.handle.value,
                .kind = ResourceKind::mesh,
                .location = ResourceLocation::imported_blob,
                .generation = 1,
                .source_path = source_path
            });
        }

        for (const auto& material : scene.materials) {
            records_.push_back(ResourceRecord{
                .id = material.handle.value,
                .kind = ResourceKind::material,
                .location = ResourceLocation::imported_blob,
                .generation = 1,
                .source_path = source_path
            });
        }

        return {};
    }

    [[nodiscard]] std::expected<std::vector<ResourceRecord>, vve::Error> enumerate() const override {
        return records_;
    }

    [[nodiscard]] std::expected<void, vve::Error> uploadResources(
        const FrameContext&,
        const SceneData&) override {
        for (auto& record : records_) {
            if (record.location == ResourceLocation::imported_blob ||
                record.location == ResourceLocation::cpu_memory) {
                record.location = ResourceLocation::gpu_memory;
                ++record.generation;
            }
        }

        return {};
    }

    void registerTasks(
        TaskGraphBuilder& builder,
        const SceneData&) override {
        const auto upload_resources_task = builder.addTask(
            "task.upload_resources",
            TaskKernelId::upload_resources,
            {},
            {TaskGraphBuilder::taskHandleFor("task.cull_visibility_cpu")},
            {},
            "Upload Resources");

        builder.setTaskCallback(
            upload_resources_task,
            [this](const TaskExecutionContext& execution_context) -> std::expected<void, vve::Error> {
                if (execution_context.frame_context == nullptr || execution_context.scene == nullptr) {
                    return std::unexpected(vve::Error::invalid_argument);
                }

                return uploadResources(*execution_context.frame_context, *execution_context.scene);
            });
    }

private:
    std::vector<ResourceRecord> records_{};
};

} // namespace

std::unique_ptr<IResourceSystem> detail::createResourceSystem() {
    return std::make_unique<ResourceSystem>();
}

} // namespace vve::v3
