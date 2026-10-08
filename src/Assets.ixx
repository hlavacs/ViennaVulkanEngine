export module VVEngine:Assets;
import std;
import :Implementation;
import VVEngine.Error;
import VVEngine.Types;

/**
	* @file
	* @brief Public asset-system facade backed by the selected engine implementation.
	*/
export namespace vve {

	template <typename... TSystems> class Engine;

	class AssetSystem {
	public:
		AssetSystem(const AssetSystem &) = default;
		AssetSystem(AssetSystem &&) noexcept = default;
		AssetSystem &operator=(const AssetSystem &) = delete;
		AssetSystem &operator=(AssetSystem &&) noexcept = delete;

		/// @brief Adds an empty scene to the selected implementation asset catalogue.
		[[nodiscard]] inline auto addScene(ObjectName name) -> std::expected<SceneHandle, Error> {
			return impl_.addScene(std::move(name));
		}
		/// @brief Loads a scene file through the selected implementation asset importer.
		[[nodiscard]] inline auto loadScene(const std::filesystem::path &source) -> std::expected<SceneHandle, Error> {
			return impl_.loadScene(source);
		}
		/// @brief Reports whether the selected implementation contains a scene handle.
		[[nodiscard]] inline bool containsScene(SceneHandle scene) const { return impl_.containsScene(scene); }
		/// @brief Returns the public name stored for a scene.
		[[nodiscard]] inline auto sceneName(SceneHandle scene) const -> std::expected<ObjectName, Error> {
			return impl_.sceneName(scene);
		}
		/// @brief Returns the number of nodes stored in a scene.
		[[nodiscard]] inline auto sceneNodeCount(SceneHandle scene) const -> std::expected<std::size_t, Error> {
			return impl_.sceneNodeCount(scene);
		}
		/// @brief Returns the number of meshes stored in a scene.
		[[nodiscard]] inline auto sceneMeshCount(SceneHandle scene) const -> std::expected<std::size_t, Error> {
			return impl_.sceneMeshCount(scene);
		}
		/// @brief Returns the number of materials stored in a scene.
		[[nodiscard]] inline auto sceneMaterialCount(SceneHandle scene) const -> std::expected<std::size_t, Error> {
			return impl_.sceneMaterialCount(scene);
		}
		/// @brief Returns the number of distinct texture source paths referenced by a scene's materials.
		[[nodiscard]] inline auto sceneTextureCount(SceneHandle scene) const -> std::expected<std::size_t, Error> {
			return impl_.sceneTextureCount(scene);
		}
		/// @brief Returns the number of lights stored in a scene.
		[[nodiscard]] inline auto sceneLightCount(SceneHandle scene) const -> std::expected<std::size_t, Error> {
			return impl_.sceneLightCount(scene);
		}
		/// @brief Returns the number of cameras stored in a scene.
		[[nodiscard]] inline auto sceneCameraCount(SceneHandle scene) const -> std::expected<std::size_t, Error> {
			return impl_.sceneCameraCount(scene);
		}
		/// @brief Returns the root node of a scene hierarchy.
		[[nodiscard]] inline auto sceneRootNode(SceneHandle scene) const -> std::expected<NodeHandle, Error> {
			return impl_.sceneRootNode(scene);
		}
		/// @brief Returns all node handles stored in a scene.
		[[nodiscard]] inline auto sceneNodes(SceneHandle scene) const -> std::expected<Vector<NodeHandle>, Error> {
			return impl_.sceneNodes(scene);
		}
		/// @brief Returns all mesh handles stored in a scene.
		[[nodiscard]] inline auto sceneMeshes(SceneHandle scene) const -> std::expected<Vector<MeshHandle>, Error> {
			return impl_.sceneMeshes(scene);
		}
		/// @brief Returns all material handles stored in a scene.
		[[nodiscard]] inline auto sceneMaterials(SceneHandle scene) const -> std::expected<Vector<MaterialHandle>, Error> {
			return impl_.sceneMaterials(scene);
		}
		/// @brief Returns all light handles stored in a scene.
		[[nodiscard]] inline auto sceneLights(SceneHandle scene) const -> std::expected<Vector<LightHandle>, Error> {
			return impl_.sceneLights(scene);
		}
		/// @brief Returns all camera handles stored in a scene.
		[[nodiscard]] inline auto sceneCameras(SceneHandle scene) const -> std::expected<Vector<CameraHandle>, Error> {
			return impl_.sceneCameras(scene);
		}
		/// @brief Returns public descriptor data for an imported light.
		[[nodiscard]] inline auto lightData(LightHandle light) const -> std::expected<LightDescriptor, Error> {
			return impl_.lightData(light);
		}
		/// @brief Returns public descriptor data for an imported camera.
		[[nodiscard]] inline auto cameraData(CameraHandle camera) const -> std::expected<CameraDescriptor, Error> {
			return impl_.cameraData(camera);
		}
		/// @brief Returns the children of a node in a scene hierarchy.
		[[nodiscard]] inline std::expected<Vector<NodeHandle>, Error> sceneNodeChildren(SceneHandle scene,
			NodeHandle node) const {
			return impl_.sceneNodeChildren(scene, node);
		}
		/// @brief Returns the parent of a node in a scene hierarchy when present.
		[[nodiscard]] inline std::expected<std::optional<NodeHandle>, Error> sceneNodeParent(SceneHandle scene,
			NodeHandle node) const {
			return impl_.sceneNodeParent(scene, node);
		}

		/// @brief Returns the public name stored for a node.
		[[nodiscard]] inline std::expected<ObjectName, Error> nodeName(NodeHandle node) const {
			return impl_.nodeName(node);
		}
		/// @brief Returns the transform stored for a node.
		[[nodiscard]] inline auto nodeTransform(NodeHandle node) const -> std::expected<Transform, Error> {
			return impl_.nodeTransform(node);
		}
		/// @brief Returns the meshes referenced by a node.
		[[nodiscard]] inline auto nodeMeshes(NodeHandle node) const -> std::expected<Vector<MeshHandle>, Error> {
			return impl_.nodeMeshes(node);
		}
		/// @brief Returns the materials referenced by a node.
		[[nodiscard]] inline auto nodeMaterials(NodeHandle node) const -> std::expected<Vector<MaterialHandle>, Error> {
			return impl_.nodeMaterials(node);
		}

		/// @brief Returns the public name stored for a mesh.
		[[nodiscard]] inline std::expected<ObjectName, Error> meshName(MeshHandle mesh) const {
			return impl_.meshName(mesh);
		}
		/// @brief Returns the number of vertices stored for a mesh.
		[[nodiscard]] inline auto meshVertexCount(MeshHandle mesh) const -> std::expected<VertexCount, Error> {
			return impl_.meshVertexCount(mesh);
		}
		/// @brief Returns the number of indices stored for a mesh.
		[[nodiscard]] inline auto meshIndexCount(MeshHandle mesh) const -> std::expected<IndexCount, Error> {
			return impl_.meshIndexCount(mesh);
		}
		/// @brief Returns the material assigned to a mesh.
		[[nodiscard]] inline auto meshMaterial(MeshHandle mesh) const -> std::expected<MaterialHandle, Error> {
			return impl_.meshMaterial(mesh);
		}
		/// @brief Returns the bounds stored for a mesh.
		[[nodiscard]] inline std::expected<Bounds, Error> meshBounds(MeshHandle mesh) const {
			return impl_.meshBounds(mesh);
		}
		/// @brief Returns the vertex positions stored for a mesh.
		[[nodiscard]] inline auto meshPositions(MeshHandle mesh) const -> std::expected<Vector<Vec3>, Error> {
			return impl_.meshPositions(mesh);
		}
		/// @brief Returns the vertex normals stored for a mesh.
		[[nodiscard]] inline auto meshNormals(MeshHandle mesh) const -> std::expected<Vector<Vec3>, Error> {
			return impl_.meshNormals(mesh);
		}
		/// @brief Returns the texture coordinates stored for a mesh.
		[[nodiscard]] inline auto meshTexcoords(MeshHandle mesh) const -> std::expected<Vector<Vec2>, Error> {
			return impl_.meshTexcoords(mesh);
		}
		/// @brief Returns the indices stored for a mesh.
		[[nodiscard]] inline auto meshIndices(MeshHandle mesh) const -> std::expected<Vector<std::uint32_t>, Error> {
			return impl_.meshIndices(mesh);
		}

		/// @brief Returns the public name stored for a material.
		[[nodiscard]] inline auto materialName(MaterialHandle material) const -> std::expected<ObjectName, Error> {
			return impl_.materialName(material);
		}

	private:
		template <typename... TSystems> friend class Engine;

		using Impl = detail::AssetSystemImpl;	///< Wrapped implementation class.
		/// @brief Binds the facade wrapper to the implementation object owned by the engine.
		inline explicit AssetSystem(Impl &implementation) noexcept : impl_{implementation} {}

		Impl &impl_;	///< Non-owning reference to the wrapped implementation.
	};	///< Public asset-system wrapper.

} // namespace vve
