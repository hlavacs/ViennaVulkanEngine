module VEEngine.V3;
import std;
#include "V3Internal.hpp"

namespace vve::v3 {

namespace {

class AssimpAssetSystem final : public IAssetSystem {
public:
    [[nodiscard]] std::string_view name() const noexcept override {
        return "AssimpAssetSystem";
    }

    [[nodiscard]] std::expected<ImportedScene, vve::Error> importScene(
        const std::filesystem::path& source_path) override {
        ImportedScene scene{};
        scene.handle = SceneHandle{detail::makeStableHandle(source_path.string())};
        scene.name = source_path.filename().string();
        scene.meshes.push_back(ImportedMesh{
            .handle = MeshHandle{detail::makeStableHandle(scene.name, 1)},
            .name = std::format("{}_mesh", scene.name)
        });
        scene.materials.push_back(ImportedMaterial{
            .handle = MaterialHandle{detail::makeStableHandle(scene.name, 2)},
            .name = std::format("{}_material", scene.name)
        });
        scene.nodes.push_back(ImportedSceneNode{
            .handle = SceneNodeHandle{detail::makeStableHandle(scene.name, 3)},
            .parent = {},
            .name = scene.name.empty() ? "Root" : scene.name
        });
        return scene;
    }
};

} // namespace

std::unique_ptr<IAssetSystem> detail::createAssetSystem() {
    return std::make_unique<AssimpAssetSystem>();
}

} // namespace vve::v3
