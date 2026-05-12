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
      [[nodiscard]] std::expected<NodeHandle, Error> sceneRootNode(SceneHandle scene) const {
         return impl_.sceneRootNode(scene);
      }
      [[nodiscard]] std::expected<Vector<NodeHandle>, Error> sceneNodes(SceneHandle scene) const {
         return facadeVector<NodeHandle>(impl_.sceneNodes(scene));
      }
      [[nodiscard]] std::expected<Vector<MeshHandle>, Error> sceneMeshes(SceneHandle scene) const {
         return facadeVector<MeshHandle>(impl_.sceneMeshes(scene));
      }
      [[nodiscard]] std::expected<Vector<MaterialHandle>, Error> sceneMaterials(SceneHandle scene) const {
         return facadeVector<MaterialHandle>(impl_.sceneMaterials(scene));
      }
      [[nodiscard]] std::expected<Vector<TextureHandle>, Error> sceneTextures(SceneHandle scene) const {
         return facadeVector<TextureHandle>(impl_.sceneTextures(scene));
      }
      [[nodiscard]] std::expected<Vector<LightHandle>, Error> sceneLights(SceneHandle scene) const {
         return facadeVector<LightHandle>(impl_.sceneLights(scene));
      }
      [[nodiscard]] std::expected<Vector<CameraHandle>, Error> sceneCameras(SceneHandle scene) const {
         return facadeVector<CameraHandle>(impl_.sceneCameras(scene));
      }
      [[nodiscard]] std::expected<Vector<NodeHandle>, Error> sceneNodeChildren(SceneHandle scene,
                                                                               NodeHandle node) const {
         return facadeVector<NodeHandle>(impl_.sceneNodeChildren(scene, node));
      }
      [[nodiscard]] std::expected<std::optional<NodeHandle>, Error> sceneNodeParent(SceneHandle scene,
                                                                                    NodeHandle node) const {
         return impl_.sceneNodeParent(scene, node);
      }

      [[nodiscard]] std::expected<ObjectName, Error> nodeName(NodeHandle node) const { return impl_.nodeName(node); }
      [[nodiscard]] std::expected<Transform, Error> nodeTransform(NodeHandle node) const {
         return impl_.nodeTransform(node);
      }
      [[nodiscard]] std::expected<Vector<MeshHandle>, Error> nodeMeshes(NodeHandle node) const {
         return facadeVector<MeshHandle>(impl_.nodeMeshes(node));
      }
      [[nodiscard]] std::expected<Vector<MaterialHandle>, Error> nodeMaterials(NodeHandle node) const {
         return facadeVector<MaterialHandle>(impl_.nodeMaterials(node));
      }

      [[nodiscard]] std::expected<ObjectName, Error> meshName(MeshHandle mesh) const { return impl_.meshName(mesh); }
      [[nodiscard]] std::expected<VertexCount, Error> meshVertexCount(MeshHandle mesh) const {
         return impl_.meshVertexCount(mesh);
      }
      [[nodiscard]] std::expected<IndexCount, Error> meshIndexCount(MeshHandle mesh) const {
         return impl_.meshIndexCount(mesh);
      }
      [[nodiscard]] std::expected<MaterialHandle, Error> meshMaterial(MeshHandle mesh) const {
         return impl_.meshMaterial(mesh);
      }
      [[nodiscard]] std::expected<Bounds, Error> meshBounds(MeshHandle mesh) const { return impl_.meshBounds(mesh); }
      [[nodiscard]] std::expected<Vector<Vec3>, Error> meshPositions(MeshHandle mesh) const {
         return facadeVector<Vec3>(impl_.meshPositions(mesh));
      }
      [[nodiscard]] std::expected<Vector<Vec3>, Error> meshNormals(MeshHandle mesh) const {
         return facadeVector<Vec3>(impl_.meshNormals(mesh));
      }
      [[nodiscard]] std::expected<Vector<Vec2>, Error> meshTexcoords(MeshHandle mesh) const {
         return facadeVector<Vec2>(impl_.meshTexcoords(mesh));
      }
      [[nodiscard]] std::expected<Vector<std::uint32_t>, Error> meshIndices(MeshHandle mesh) const {
         return facadeVector<std::uint32_t>(impl_.meshIndices(mesh));
      }

      [[nodiscard]] std::expected<ObjectName, Error> materialName(MaterialHandle material) const {
         return impl_.materialName(material);
      }
      [[nodiscard]] std::expected<Vector<TextureHandle>, Error> materialTextures(MaterialHandle material) const {
         return facadeVector<TextureHandle>(impl_.materialTextures(material));
      }

   private:
      template <typename T>
      [[nodiscard]] static std::expected<Vector<T>, Error>
      facadeVector(std::expected<typename Vector<T>::implementation_type, Error> values) {
         if (!values) { return std::unexpected(values.error()); }
         return Vector<T>{std::move(*values)};
      }

      Impl &impl_;
   }; ///< Public asset-system wrapper.

} // namespace vve
