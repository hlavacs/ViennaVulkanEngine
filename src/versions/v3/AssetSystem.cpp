module VEEngine.V3;
import std;
import :Internal;

namespace vve::v3 {

class AssimpAssetSystemImplementation {
public:
    [[nodiscard]] std::string_view name() const noexcept {
        return "AssimpAssetSystem";
    }

    [[nodiscard]] std::expected<ImportedScene, vve::Error> importScene(
        const std::filesystem::path& source_path) {
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

template <>
AssetSystemFacade<AssimpAssetSystemImplementation>::AssetSystemFacade()
    : implementation_(
          new AssimpAssetSystemImplementation(),
          [](AssimpAssetSystemImplementation* implementation) {
              delete implementation;
          }) {
}

std::string_view AssetSystemFacade<AssimpAssetSystemImplementation>::name() const noexcept {
    return implementation_->name();
}

template <>
std::expected<ImportedScene, vve::Error> AssetSystemFacade<AssimpAssetSystemImplementation>::importScene(
    const std::filesystem::path& source_path) {
    return implementation_->importScene(source_path);
}

template class AssetSystemFacade<AssimpAssetSystemImplementation>;

} // namespace vve::v3
