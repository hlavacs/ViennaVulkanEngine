module;

#include "FacadeMacros.hpp"

module VEEngine.V3;
import std;
import :Internal;

/**
 * @file
 * @brief v3 scene-loading orchestration implementation.
 *
 * This file owns the workflow that imports a scene file, registers its
 * resources, and instantiates the runtime scene representation.
 */
namespace vve::v3 {

   /**
    * @brief Concrete scene-loading implementation used by v3.
    *
    * The loader coordinates asset import, resource registration, and scene
    * instantiation while leaving active-scene ownership to the engine.
    */
   class DefaultSceneLoaderImplementation {
   public:
      /**
       * @brief Creates the scene loader around the participating subsystems.
       * @param asset_system Asset-import subsystem facade.
       * @param resource_system Resource-registration subsystem facade.
       * @param scene_system Scene-instantiation subsystem facade.
       */
      DefaultSceneLoaderImplementation(AssetSystem &asset_system, ResourceSystem &resource_system,
                                       SceneSystem &scene_system)
          : asset_system_(asset_system), resource_system_(resource_system), scene_system_(scene_system) {}

      /// @brief Returns the subsystem name for diagnostics.
      [[nodiscard]] std::string_view name() const noexcept { return "SceneLoader"; }

      /**
       * @brief Imports, registers, and instantiates a scene from a source path.
       * @param file_path Scene source path requested by the caller.
       * @return Runtime scene data on success, or the first orchestration error.
       */
      [[nodiscard]] std::expected<SceneData, vve::Error> loadScene(const std::filesystem::path &file_path) {
         if (file_path.empty()) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         // Import converts source data into engine-owned intermediate scene data.
         const auto imported_scene = asset_system_.importScene(file_path);
         if (!imported_scene) {
            return std::unexpected(imported_scene.error());
         }

         // Resource registration assigns stable engine handles before instantiation.
         if (auto register_result = resource_system_.registerImportedScene(*imported_scene, file_path);
             !register_result) {
            return std::unexpected(register_result.error());
         }

         // Scene instantiation produces the runtime scene representation consumed by systems.
         return scene_system_.instantiate(*imported_scene);
      }

   private:
      AssetSystem &asset_system_;       ///< Asset-import subsystem used for source ingestion.
      ResourceSystem &resource_system_; ///< Resource subsystem used for imported-resource registration.
      SceneSystem &scene_system_;       ///< Scene subsystem used for runtime scene instantiation.
   };

   /// @brief Constructs the public scene-loader facade around the concrete implementation.
   VVE_V3_DEFINE_FACADE_CTOR(
       SceneLoaderFacade, DefaultSceneLoaderImplementation,
       (AssetSystemFacade<AssimpAssetSystemImplementation> &asset_system,
        ResourceSystemFacade<DefaultResourceSystemImplementation> &resource_system,
        SceneSystemFacade<DefaultSceneSystemImplementation> &scene_system),
       (asset_system, resource_system, scene_system))

   /// @brief Returns the scene-loader name for the public facade.
   VVE_V3_DEFINE_FACADE_METHOD(SceneLoaderFacade, DefaultSceneLoaderImplementation, name, (), (), const noexcept,
                               std::string_view)

   /// @brief Loads a runtime scene through the public scene-loader facade.
   VVE_V3_DEFINE_FACADE_METHOD(SceneLoaderFacade, DefaultSceneLoaderImplementation, loadScene,
                               (const std::filesystem::path &file_path), (file_path), ,
                               std::expected<SceneData, vve::Error>)

   /// @brief Emits the explicit scene-loader facade instantiation for v3.
   template class SceneLoaderFacade<DefaultSceneLoaderImplementation>;

} // namespace vve::v3
