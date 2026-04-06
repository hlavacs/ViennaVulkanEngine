module VEEngine.V3;
import std;
import :Internal;

/**
 * @file
 * @brief v3 asset-system implementation.
 *
 * The current implementation is intentionally minimal and synthesizes imported
 * scene data from source-path metadata. It provides the subsystem seam that a
 * fuller importer can later replace.
 */
namespace vve::v3 {

   /**
    * @brief Minimal placeholder asset importer used by the current v3 runtime.
    *
    * The implementation deliberately keeps import behavior trivial so the rest
    * of the runtime can exercise stable asset/resource seams before a full
    * importer backend is introduced.
    */
   class AssimpAssetSystemImplementation {
   public:
      /// @brief Returns the implementation name used in runtime diagnostics.
      [[nodiscard]] std::string_view name() const noexcept { return "AssimpAssetSystem"; }

      /**
       * @brief Synthesizes imported scene data from a source file path.
       * @param source_path Scene path requested by the caller.
       * @return Minimal imported scene description with one mesh, one material, and one root node.
       */
      [[nodiscard]] std::expected<ImportedScene, vve::Error> importScene(const std::filesystem::path &source_path) {
         ImportedScene scene{};
         // Stable handles let later subsystems refer to imported records
         // deterministically without depending on container indices.
         scene.handle = SceneHandle{detail::makeStableHandle(source_path.string())};
         scene.name = source_path.filename().string();
         // The placeholder importer emits one mesh and one material so the
         // downstream resource and scene systems can validate their plumbing.
         scene.meshes.push_back(ImportedMesh{.handle = MeshHandle{detail::makeStableHandle(scene.name, 1)},
                                             .name = std::format("{}_mesh", scene.name)});
         scene.materials.push_back(ImportedMaterial{.handle = MaterialHandle{detail::makeStableHandle(scene.name, 2)},
                                                    .name = std::format("{}_material", scene.name)});
         // The imported hierarchy currently collapses to a single root node.
         scene.nodes.push_back(ImportedSceneNode{.handle = SceneNodeHandle{detail::makeStableHandle(scene.name, 3)},
                                                 .parent = {},
                                                 .name = scene.name.empty() ? "Root" : scene.name});
         return scene;
      }
   };

   /// @brief Constructs the public asset-system facade around the concrete implementation.
   template <>
   AssetSystemFacade<AssimpAssetSystemImplementation>::AssetSystemFacade()
       : implementation_(new AssimpAssetSystemImplementation(),
                         [](AssimpAssetSystemImplementation *implementation) { delete implementation; }) {}

   /// @brief Returns the asset-system name for the public facade.
   std::string_view AssetSystemFacade<AssimpAssetSystemImplementation>::name() const noexcept {
      return implementation_->name();
   }

   /// @brief Imports a scene through the public asset-system facade.
   template <>
   std::expected<ImportedScene, vve::Error>
   AssetSystemFacade<AssimpAssetSystemImplementation>::importScene(const std::filesystem::path &source_path) {
      return implementation_->importScene(source_path);
   }

   /// @brief Emits the explicit asset-system facade instantiation for v3.
   template class AssetSystemFacade<AssimpAssetSystemImplementation>;

} // namespace vve::v3
