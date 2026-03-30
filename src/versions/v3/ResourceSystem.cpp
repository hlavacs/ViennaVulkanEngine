module VEEngine.V3;
import std;
#include "V3Internal.hpp"

namespace vve::v3 {

namespace {

class ResourceSystem final : public IResourceSystem {
public:
    [[nodiscard]] std::string_view name() const noexcept override {
        return "ResourceSystem";
    }

    [[nodiscard]] std::expected<void, vve::Result> registerImportedScene(
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

    [[nodiscard]] std::expected<std::vector<ResourceRecord>, vve::Result> enumerate() const override {
        return records_;
    }

private:
    std::vector<ResourceRecord> records_{};
};

} // namespace

std::unique_ptr<IResourceSystem> detail::createResourceSystem() {
    return std::make_unique<ResourceSystem>();
}

} // namespace vve::v3
