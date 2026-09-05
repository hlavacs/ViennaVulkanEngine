export module VEEngine:Assets;
import std;
import :Implementation;
import VEEngine.Error;
import VEEngine.Types;

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

		[[nodiscard]] auto addScene(ObjectName name)									-> std::expected<SceneHandle, Error>;
		[[nodiscard]] auto loadScene(const std::filesystem::path &source)		-> std::expected<SceneHandle, Error>;
		[[nodiscard]] inline std::expected<SceneHandle, Error> loadScene(const std::filesystem::path &source,
																													 const SceneLoadOptions &options) {
			return loadScene(source);
		}																										///< Accepts facade scene-load options; honored as loader support grows.
		[[nodiscard]] bool containsScene(SceneHandle scene) const;
		[[nodiscard]] auto sceneName(SceneHandle scene) const						-> std::expected<ObjectName, Error>;
		[[nodiscard]] auto sceneNodeCount(SceneHandle scene) const				-> std::expected<std::size_t, Error>;
		[[nodiscard]] auto sceneMeshCount(SceneHandle scene) const				-> std::expected<std::size_t, Error>;
		[[nodiscard]] auto sceneMaterialCount(SceneHandle scene) const			-> std::expected<std::size_t, Error>;
		[[nodiscard]] auto sceneTextureCount(SceneHandle scene) const			-> std::expected<std::size_t, Error>;
		[[nodiscard]] auto sceneLightCount(SceneHandle scene) const				-> std::expected<std::size_t, Error>;
		[[nodiscard]] auto sceneCameraCount(SceneHandle scene) const			-> std::expected<std::size_t, Error>;
		[[nodiscard]] auto sceneRootNode(SceneHandle scene) const				-> std::expected<NodeHandle, Error>;
		[[nodiscard]] auto sceneNodes(SceneHandle scene) const					-> std::expected<Vector<NodeHandle>, Error>;
		[[nodiscard]] auto sceneMeshes(SceneHandle scene) const					-> std::expected<Vector<MeshHandle>, Error>;
		[[nodiscard]] auto sceneMaterials(SceneHandle scene) const				-> std::expected<Vector<MaterialHandle>, Error>;
		[[nodiscard]] auto sceneTextures(SceneHandle scene) const				-> std::expected<Vector<TextureHandle>, Error>;
		[[nodiscard]] auto sceneLights(SceneHandle scene) const					-> std::expected<Vector<LightHandle>, Error>;
		[[nodiscard]] auto sceneCameras(SceneHandle scene) const					-> std::expected<Vector<CameraHandle>, Error>;
		[[nodiscard]] auto lightData(LightHandle light) const					-> std::expected<LightDescriptor, Error>;
		[[nodiscard]] auto cameraData(CameraHandle camera) const				-> std::expected<CameraDescriptor, Error>;
		[[nodiscard]] std::expected<Vector<NodeHandle>, Error> sceneNodeChildren(SceneHandle scene,
																											NodeHandle node) const;
		[[nodiscard]] std::expected<std::optional<NodeHandle>, Error> sceneNodeParent(SceneHandle scene,
																												NodeHandle node) const;

		[[nodiscard]] std::expected<ObjectName, Error> nodeName(NodeHandle node) const;
		[[nodiscard]] auto nodeTransform(NodeHandle node) const					-> std::expected<Transform, Error>;
		[[nodiscard]] auto nodeMeshes(NodeHandle node) const						-> std::expected<Vector<MeshHandle>, Error>;
		[[nodiscard]] auto nodeMaterials(NodeHandle node) const					-> std::expected<Vector<MaterialHandle>, Error>;

		[[nodiscard]] std::expected<ObjectName, Error> meshName(MeshHandle mesh) const;
		[[nodiscard]] auto meshVertexCount(MeshHandle mesh) const				-> std::expected<VertexCount, Error>;
		[[nodiscard]] auto meshIndexCount(MeshHandle mesh) const					-> std::expected<IndexCount, Error>;
		[[nodiscard]] auto meshMaterial(MeshHandle mesh) const					-> std::expected<MaterialHandle, Error>;
		[[nodiscard]] std::expected<Bounds, Error> meshBounds(MeshHandle mesh) const;
		[[nodiscard]] auto meshPositions(MeshHandle mesh) const					-> std::expected<Vector<Vec3>, Error>;
		[[nodiscard]] auto meshNormals(MeshHandle mesh) const						-> std::expected<Vector<Vec3>, Error>;
		[[nodiscard]] auto meshTexcoords(MeshHandle mesh) const					-> std::expected<Vector<Vec2>, Error>;
		[[nodiscard]] auto meshIndices(MeshHandle mesh) const						-> std::expected<Vector<std::uint32_t>, Error>;

		[[nodiscard]] auto materialName(MaterialHandle material) const			-> std::expected<ObjectName, Error>;
		[[nodiscard]] auto materialTextures(MaterialHandle material) const	-> std::expected<Vector<TextureHandle>, Error>;

	private:
		template <typename... TSystems> friend class Engine;

		using Impl = detail::AssetSystemImpl;	///< Wrapped implementation class.
		explicit AssetSystem(Impl &implementation) noexcept;

		Impl &impl_;	///< Non-owning reference to the wrapped implementation.
	};	///< Public asset-system wrapper.

} // namespace vve
