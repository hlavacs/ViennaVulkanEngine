export module VEEngine:Assets;
import std;
import VEEngine.V4;
import VEEngine.Error;
import VEEngine.Types;

/**
 * @file
 * @brief Public asset-system facade backed by the selected engine implementation.
 */
export namespace vve {

   class AssetSystem {
      using Impl = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::AssetSystem;

   public:
      explicit AssetSystem(Impl &implementation) : impl_{implementation} {}
      AssetSystem(const AssetSystem &) = default;
      AssetSystem(AssetSystem &&) noexcept = default;
      AssetSystem &operator=(const AssetSystem &) = delete;
      AssetSystem &operator=(AssetSystem &&) noexcept = delete;

      [[nodiscard]] std::expected<SceneHandle, Error> addScene(ObjectName name) {
         return impl_.addScene(std::move(name));
      }
      [[nodiscard]] std::expected<SceneHandle, Error> loadScene(const std::filesystem::path &source) {
         return impl_.loadScene(source);
      }
      [[nodiscard]] bool containsScene(SceneHandle scene) const { return impl_.containsScene(scene); }
      [[nodiscard]] std::expected<ObjectName, Error> sceneName(SceneHandle scene) const {
         return impl_.sceneName(scene);
      }
      [[nodiscard]] std::expected<std::size_t, Error> sceneNodeCount(SceneHandle scene) const {
         return impl_.sceneNodeCount(scene);
      }
      [[nodiscard]] std::expected<std::size_t, Error> sceneMeshCount(SceneHandle scene) const {
         return impl_.sceneMeshCount(scene);
      }
      [[nodiscard]] std::expected<std::size_t, Error> sceneMaterialCount(SceneHandle scene) const {
         return impl_.sceneMaterialCount(scene);
      }
      [[nodiscard]] std::expected<std::size_t, Error> sceneTextureCount(SceneHandle scene) const {
         return impl_.sceneTextureCount(scene);
      }
      [[nodiscard]] std::expected<std::size_t, Error> sceneLightCount(SceneHandle scene) const {
         return impl_.sceneLightCount(scene);
      }
      [[nodiscard]] std::expected<std::size_t, Error> sceneCameraCount(SceneHandle scene) const {
         return impl_.sceneCameraCount(scene);
      }

   private:
      Impl &impl_;
   }; ///< Public asset-system wrapper.

} // namespace vve
