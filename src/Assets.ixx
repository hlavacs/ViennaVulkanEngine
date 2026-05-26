export module VEEngine:Assets;
import std;
#if defined(VVE_ENGINE_IMPLEMENTATION_IS_V5)
import VEEngine.V5;
#else
import VEEngine.V4;
#endif
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

		[[nodiscard]] auto addScene(ObjectName name)									-> std::expected<SceneHandle, Error>{
			return impl_.addScene(std::move(name));
		}
		[[nodiscard]] auto loadScene(const std::filesystem::path &source)		-> std::expected<SceneHandle, Error>{
			return impl_.loadScene(source);
		}
		[[nodiscard]] bool containsScene(SceneHandle scene) const { return impl_.containsScene(scene); }
		[[nodiscard]] auto sceneName(SceneHandle scene) const						-> std::expected<ObjectName, Error>{
			return impl_.sceneName(scene);
		}
		[[nodiscard]] auto sceneNodeCount(SceneHandle scene) const				-> std::expected<std::size_t, Error>{
			return impl_.sceneNodeCount(scene);
		}
		[[nodiscard]] auto sceneMeshCount(SceneHandle scene) const				-> std::expected<std::size_t, Error>{
			return impl_.sceneMeshCount(scene);
		}
		[[nodiscard]] auto sceneMaterialCount(SceneHandle scene) const			-> std::expected<std::size_t, Error>{
			return impl_.sceneMaterialCount(scene);
		}
		[[nodiscard]] auto sceneTextureCount(SceneHandle scene) const			-> std::expected<std::size_t, Error>{
			return impl_.sceneTextureCount(scene);
		}
		[[nodiscard]] auto sceneLightCount(SceneHandle scene) const				-> std::expected<std::size_t, Error>{
			return impl_.sceneLightCount(scene);
		}
		[[nodiscard]] auto sceneCameraCount(SceneHandle scene) const			-> std::expected<std::size_t, Error>{
			return impl_.sceneCameraCount(scene);
		}
		[[nodiscard]] auto sceneRootNode(SceneHandle scene) const				-> std::expected<NodeHandle, Error>{
			return impl_.sceneRootNode(scene);
		}
		[[nodiscard]] auto sceneNodes(SceneHandle scene) const					-> std::expected<Vector<NodeHandle>, Error>{
			return facadeVector<NodeHandle>(impl_.sceneNodes(scene));
		}
		[[nodiscard]] auto sceneMeshes(SceneHandle scene) const					-> std::expected<Vector<MeshHandle>, Error>{
			return facadeVector<MeshHandle>(impl_.sceneMeshes(scene));
		}
		[[nodiscard]] auto sceneMaterials(SceneHandle scene) const				-> std::expected<Vector<MaterialHandle>, Error>{
			return facadeVector<MaterialHandle>(impl_.sceneMaterials(scene));
		}
		[[nodiscard]] auto sceneTextures(SceneHandle scene) const				-> std::expected<Vector<TextureHandle>, Error>{
			return facadeVector<TextureHandle>(impl_.sceneTextures(scene));
		}
		[[nodiscard]] auto sceneLights(SceneHandle scene) const					-> std::expected<Vector<LightHandle>, Error>{
			return facadeVector<LightHandle>(impl_.sceneLights(scene));
		}
		[[nodiscard]] auto sceneCameras(SceneHandle scene) const					-> std::expected<Vector<CameraHandle>, Error>{
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
		[[nodiscard]] auto nodeTransform(NodeHandle node) const					-> std::expected<Transform, Error>{
			return impl_.nodeTransform(node);
		}
		[[nodiscard]] auto nodeMeshes(NodeHandle node) const						-> std::expected<Vector<MeshHandle>, Error>{
			return facadeVector<MeshHandle>(impl_.nodeMeshes(node));
		}
		[[nodiscard]] auto nodeMaterials(NodeHandle node) const					-> std::expected<Vector<MaterialHandle>, Error>{
			return facadeVector<MaterialHandle>(impl_.nodeMaterials(node));
		}

		[[nodiscard]] std::expected<ObjectName, Error> meshName(MeshHandle mesh) const { return impl_.meshName(mesh); }
		[[nodiscard]] auto meshVertexCount(MeshHandle mesh) const				-> std::expected<VertexCount, Error>{
			return impl_.meshVertexCount(mesh);
		}
		[[nodiscard]] auto meshIndexCount(MeshHandle mesh) const					-> std::expected<IndexCount, Error>{
			return impl_.meshIndexCount(mesh);
		}
		[[nodiscard]] auto meshMaterial(MeshHandle mesh) const					-> std::expected<MaterialHandle, Error>{
			return impl_.meshMaterial(mesh);
		}
		[[nodiscard]] std::expected<Bounds, Error> meshBounds(MeshHandle mesh) const { return impl_.meshBounds(mesh); }
		[[nodiscard]] auto meshPositions(MeshHandle mesh) const					-> std::expected<Vector<Vec3>, Error>{
			return facadeVector<Vec3>(impl_.meshPositions(mesh));
		}
		[[nodiscard]] auto meshNormals(MeshHandle mesh) const						-> std::expected<Vector<Vec3>, Error>{
			return facadeVector<Vec3>(impl_.meshNormals(mesh));
		}
		[[nodiscard]] auto meshTexcoords(MeshHandle mesh) const					-> std::expected<Vector<Vec2>, Error>{
			return facadeVector<Vec2>(impl_.meshTexcoords(mesh));
		}
		[[nodiscard]] auto meshIndices(MeshHandle mesh) const						-> std::expected<Vector<std::uint32_t>, Error>{
			return facadeVector<std::uint32_t>(impl_.meshIndices(mesh));
		}

		[[nodiscard]] auto materialName(MaterialHandle material) const			-> std::expected<ObjectName, Error>{
			return impl_.materialName(material);
		}
		[[nodiscard]] auto materialTextures(MaterialHandle material) const	-> std::expected<Vector<TextureHandle>, Error>{
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
	};	///< Public asset-system wrapper.

} // namespace vve
