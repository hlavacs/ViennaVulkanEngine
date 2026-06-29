module VEEngine;
import :Assets;
import VEEngine.Simple;

namespace vve {

	namespace {
		/// @brief Recovers the selected implementation asset system from the erased facade pointer.
		[[nodiscard]] auto assetSystemImpl(void *implementation) -> VVE_ENGINE_IMPLEMENTATION_NAMESPACE::AssetSystem & {
			return *static_cast<VVE_ENGINE_IMPLEMENTATION_NAMESPACE::AssetSystem *>(implementation);
		}

		/// @brief Converts implementation-owned vectors into facade vectors while preserving errors.
		template <typename T>
		[[nodiscard]] std::expected<Vector<T>, Error>
		facadeVector(std::expected<typename Vector<T>::implementation_type, Error> values) {
			if (!values) { return std::unexpected(values.error()); }
			return Vector<T>{std::move(*values)};
		}
	} // namespace

	/// @brief Stores the erased implementation reference used by engine-owned asset systems.
	AssetSystem::AssetSystem(void *implementation) noexcept : impl_{implementation} {}

	/// @brief Adds an empty scene to the selected implementation asset catalogue.
	auto AssetSystem::addScene(ObjectName name) -> std::expected<SceneHandle, Error> {
		return assetSystemImpl(impl_).addScene(std::move(name));
	}

	/// @brief Loads a scene file through the selected implementation asset importer.
	auto AssetSystem::loadScene(const std::filesystem::path &source) -> std::expected<SceneHandle, Error> {
		return assetSystemImpl(impl_).loadScene(source);
	}

	/// @brief Reports whether the selected implementation contains a scene handle.
	bool AssetSystem::containsScene(SceneHandle scene) const { return assetSystemImpl(impl_).containsScene(scene); }

	/// @brief Returns the public name stored for a scene.
	auto AssetSystem::sceneName(SceneHandle scene) const -> std::expected<ObjectName, Error> {
		return assetSystemImpl(impl_).sceneName(scene);
	}

	/// @brief Returns the number of nodes stored in a scene.
	auto AssetSystem::sceneNodeCount(SceneHandle scene) const -> std::expected<std::size_t, Error> {
		return assetSystemImpl(impl_).sceneNodeCount(scene);
	}

	/// @brief Returns the number of meshes stored in a scene.
	auto AssetSystem::sceneMeshCount(SceneHandle scene) const -> std::expected<std::size_t, Error> {
		return assetSystemImpl(impl_).sceneMeshCount(scene);
	}

	/// @brief Returns the number of materials stored in a scene.
	auto AssetSystem::sceneMaterialCount(SceneHandle scene) const -> std::expected<std::size_t, Error> {
		return assetSystemImpl(impl_).sceneMaterialCount(scene);
	}

	/// @brief Returns the number of textures stored in a scene.
	auto AssetSystem::sceneTextureCount(SceneHandle scene) const -> std::expected<std::size_t, Error> {
		return assetSystemImpl(impl_).sceneTextureCount(scene);
	}

	/// @brief Returns the number of lights stored in a scene.
	auto AssetSystem::sceneLightCount(SceneHandle scene) const -> std::expected<std::size_t, Error> {
		return assetSystemImpl(impl_).sceneLightCount(scene);
	}

	/// @brief Returns the number of cameras stored in a scene.
	auto AssetSystem::sceneCameraCount(SceneHandle scene) const -> std::expected<std::size_t, Error> {
		return assetSystemImpl(impl_).sceneCameraCount(scene);
	}

	/// @brief Returns the root node of a scene hierarchy.
	auto AssetSystem::sceneRootNode(SceneHandle scene) const -> std::expected<NodeHandle, Error> {
		return assetSystemImpl(impl_).sceneRootNode(scene);
	}

	/// @brief Returns all node handles stored in a scene.
	auto AssetSystem::sceneNodes(SceneHandle scene) const -> std::expected<Vector<NodeHandle>, Error> {
		return facadeVector<NodeHandle>(assetSystemImpl(impl_).sceneNodes(scene));
	}

	/// @brief Returns all mesh handles stored in a scene.
	auto AssetSystem::sceneMeshes(SceneHandle scene) const -> std::expected<Vector<MeshHandle>, Error> {
		return facadeVector<MeshHandle>(assetSystemImpl(impl_).sceneMeshes(scene));
	}

	/// @brief Returns all material handles stored in a scene.
	auto AssetSystem::sceneMaterials(SceneHandle scene) const -> std::expected<Vector<MaterialHandle>, Error> {
		return facadeVector<MaterialHandle>(assetSystemImpl(impl_).sceneMaterials(scene));
	}

	/// @brief Returns all texture handles stored in a scene.
	auto AssetSystem::sceneTextures(SceneHandle scene) const -> std::expected<Vector<TextureHandle>, Error> {
		return facadeVector<TextureHandle>(assetSystemImpl(impl_).sceneTextures(scene));
	}

	/// @brief Returns all light handles stored in a scene.
	auto AssetSystem::sceneLights(SceneHandle scene) const -> std::expected<Vector<LightHandle>, Error> {
		return facadeVector<LightHandle>(assetSystemImpl(impl_).sceneLights(scene));
	}

	/// @brief Returns all camera handles stored in a scene.
	auto AssetSystem::sceneCameras(SceneHandle scene) const -> std::expected<Vector<CameraHandle>, Error> {
		return facadeVector<CameraHandle>(assetSystemImpl(impl_).sceneCameras(scene));
	}

	/// @brief Returns the children of a node in a scene hierarchy.
	std::expected<Vector<NodeHandle>, Error> AssetSystem::sceneNodeChildren(SceneHandle scene,
																								  NodeHandle node) const {
		return facadeVector<NodeHandle>(assetSystemImpl(impl_).sceneNodeChildren(scene, node));
	}

	/// @brief Returns the parent of a node in a scene hierarchy when present.
	std::expected<std::optional<NodeHandle>, Error> AssetSystem::sceneNodeParent(SceneHandle scene,
																										  NodeHandle node) const {
		return assetSystemImpl(impl_).sceneNodeParent(scene, node);
	}

	/// @brief Returns the public name stored for a node.
	std::expected<ObjectName, Error> AssetSystem::nodeName(NodeHandle node) const {
		return assetSystemImpl(impl_).nodeName(node);
	}

	/// @brief Returns the transform stored for a node.
	auto AssetSystem::nodeTransform(NodeHandle node) const -> std::expected<Transform, Error> {
		return assetSystemImpl(impl_).nodeTransform(node);
	}

	/// @brief Returns the meshes referenced by a node.
	auto AssetSystem::nodeMeshes(NodeHandle node) const -> std::expected<Vector<MeshHandle>, Error> {
		return facadeVector<MeshHandle>(assetSystemImpl(impl_).nodeMeshes(node));
	}

	/// @brief Returns the materials referenced by a node.
	auto AssetSystem::nodeMaterials(NodeHandle node) const -> std::expected<Vector<MaterialHandle>, Error> {
		return facadeVector<MaterialHandle>(assetSystemImpl(impl_).nodeMaterials(node));
	}

	/// @brief Returns the public name stored for a mesh.
	std::expected<ObjectName, Error> AssetSystem::meshName(MeshHandle mesh) const {
		return assetSystemImpl(impl_).meshName(mesh);
	}

	/// @brief Returns the number of vertices stored for a mesh.
	auto AssetSystem::meshVertexCount(MeshHandle mesh) const -> std::expected<VertexCount, Error> {
		return assetSystemImpl(impl_).meshVertexCount(mesh);
	}

	/// @brief Returns the number of indices stored for a mesh.
	auto AssetSystem::meshIndexCount(MeshHandle mesh) const -> std::expected<IndexCount, Error> {
		return assetSystemImpl(impl_).meshIndexCount(mesh);
	}

	/// @brief Returns the material assigned to a mesh.
	auto AssetSystem::meshMaterial(MeshHandle mesh) const -> std::expected<MaterialHandle, Error> {
		return assetSystemImpl(impl_).meshMaterial(mesh);
	}

	/// @brief Returns the bounds stored for a mesh.
	std::expected<Bounds, Error> AssetSystem::meshBounds(MeshHandle mesh) const {
		return assetSystemImpl(impl_).meshBounds(mesh);
	}

	/// @brief Returns the vertex positions stored for a mesh.
	auto AssetSystem::meshPositions(MeshHandle mesh) const -> std::expected<Vector<Vec3>, Error> {
		return facadeVector<Vec3>(assetSystemImpl(impl_).meshPositions(mesh));
	}

	/// @brief Returns the vertex normals stored for a mesh.
	auto AssetSystem::meshNormals(MeshHandle mesh) const -> std::expected<Vector<Vec3>, Error> {
		return facadeVector<Vec3>(assetSystemImpl(impl_).meshNormals(mesh));
	}

	/// @brief Returns the texture coordinates stored for a mesh.
	auto AssetSystem::meshTexcoords(MeshHandle mesh) const -> std::expected<Vector<Vec2>, Error> {
		return facadeVector<Vec2>(assetSystemImpl(impl_).meshTexcoords(mesh));
	}

	/// @brief Returns the indices stored for a mesh.
	auto AssetSystem::meshIndices(MeshHandle mesh) const -> std::expected<Vector<std::uint32_t>, Error> {
		return facadeVector<std::uint32_t>(assetSystemImpl(impl_).meshIndices(mesh));
	}

	/// @brief Returns the public name stored for a material.
	auto AssetSystem::materialName(MaterialHandle material) const -> std::expected<ObjectName, Error> {
		return assetSystemImpl(impl_).materialName(material);
	}

	/// @brief Returns the textures referenced by a material.
	auto AssetSystem::materialTextures(MaterialHandle material) const -> std::expected<Vector<TextureHandle>, Error> {
		return facadeVector<TextureHandle>(assetSystemImpl(impl_).materialTextures(material));
	}

} // namespace vve
