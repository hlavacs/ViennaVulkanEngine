module;

#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <cmath>
#include <cstdlib>

export module VVEngine.Simple:Assets;
import std;
export import VVEngine.Simple.Types;
import :Graph;

/// @file
/// @brief Compact Assimp-backed asset system for the simple engine.

namespace vve::simple {

	using SceneTree = Tree<NodeHandle>;											///< Asset scene tree over imported node handles.

	/// @brief Minimal handle table used by all imported descriptor types.
	template <typename T> struct Table {
		using Handle = typename T::Handle;																	///< Strong handle type accepted by this table.
		std::map<Handle, T> data{};																			///< Ordered descriptor storage.

		[[nodiscard]] auto add(T value)																				-> std::expected<void, Error>;
		[[nodiscard]] const T *find(Handle handle) const;
		[[nodiscard]] auto contains(Handle handle) const														-> bool;
	};

	/// @brief Scene node descriptor.
	struct Node {
		using Handle = NodeHandle;																				///< Handle category.
		NodeHandle handle{};																						///< Stable node handle.
		ObjectName name{};																						///< Imported node name.
		Transform transform{};																					///< Local transform.
		Vector<MeshHandle> meshes{};																			///< Meshes attached to this node.
	};

	/// @brief Mesh descriptor with just enough information for examples and future upload.
	struct AssetMesh {
		using Handle = MeshHandle;																				///< Handle category.
		MeshHandle handle{};																						///< Stable mesh handle.
		ObjectName name{};																						///< Imported mesh name.
		MaterialHandle material{};																				///< Default material.
		Bounds bounds{};																							///< Object-space bounds.
		std::shared_ptr<const std::vector<RenderVertex>> vertices{};		///< Imported vertices shared with render meshes.
		Vector<std::uint32_t> indices{};																		///< Imported triangle indices.
	};

	/// @brief Material descriptor containing imported factors and texture references.
	struct Material {
		using Handle = MaterialHandle;																		///< Handle category.
		MaterialHandle handle{};																				///< Stable material handle.
		ObjectName name{};																						///< Imported material name.
		LinearColor base_color{.value = oneVec3()};										///< Imported base-color factor.
		MaterialFactors factors{};																///< Imported roughness, metalness, and emissive factors.
		Vector<MaterialTextureSource> texture_sources{};									///< Typed texture sources used by rendering, at most one per semantic.
	};

	/// @brief Imported light descriptor keyed by its public handle.
	struct Light {
		using Handle = LightHandle;																			///< Handle category.
		LightHandle handle{};																					///< Stable light handle.
		LightDescriptor data{};																				///< Facade-safe imported light data.
	};

	/// @brief Imported camera descriptor keyed by its public handle.
	struct CameraAsset {
		using Handle = CameraHandle;																			///< Handle category.
		CameraHandle handle{};																				///< Stable camera handle.
		CameraDescriptor data{};																				///< Facade-safe imported camera data.
	};

	/// @brief Scene descriptor stores handle lists; details live in the catalog tables.
	struct AssetScene {
		using Handle = SceneHandle;																			///< Handle category.
		SceneHandle handle{};																					///< Stable scene handle.
		ObjectName name{};																						///< Source file name.
		SceneTree tree{};																							///< Parent/child node topology.
		Vector<MeshHandle> meshes{};																			///< All mesh handles.
		Vector<MaterialHandle> materials{};																	///< All material handles.
		Vector<LightHandle> lights{};																			///< All light handles.
		Vector<CameraHandle> cameras{};																		///< All camera handles.
	};

	/// @brief All imported descriptors, keyed by stable typed handles.
	struct Catalog {
		Table<AssetScene> scenes{};																					///< Scenes by handle.
		Table<Node> nodes{};																						///< Nodes by handle.
		Table<AssetMesh> meshes{};																					///< Meshes by handle.
		Table<Material> materials{};																			///< Materials by handle.
		Table<Light> lights{};																					///< Lights by handle.
		Table<CameraAsset> cameras{};																		///< Cameras by handle.
	};

} // namespace vve::simple

namespace vve::simple {

	/// @brief Stores one descriptor by its own handle.
	template <typename T> std::expected<void, Error> Table<T>::add(T value) {
		if (!value.handle.valid()) { return std::unexpected(Error::invalid_handle); }
		if (auto [_, ok] = data.emplace(value.handle, std::move(value)); !ok) {
			return std::unexpected(Error::duplicate_object);
		}
		return {};
	}

	/// @brief Finds a descriptor or returns nullptr.
	template <typename T> const T *Table<T>::find(typename Table<T>::Handle handle) const {
		const auto it = data.find(handle);
		return it == data.end() ? nullptr : std::addressof(it->second);
	}

	/// @brief Tests table membership.
	template <typename T> bool Table<T>::contains(typename Table<T>::Handle handle) const {
		return data.contains(handle);
	}

} // namespace vve::simple

export namespace vve::simple {

	/// @brief Asset facade that owns imported scene descriptors.
	class AssetSystem {
	public:
		[[nodiscard]] auto addScene(ObjectName name)											-> std::expected<SceneHandle, Error>;
		[[nodiscard]] auto loadScene(const std::filesystem::path &source)				-> std::expected<SceneHandle, Error>;

		// Catalog queries return Error::missing_object for unknown handles.
		[[nodiscard]] bool containsScene(SceneHandle scene) const { return catalog_.scenes.contains(scene); }
		/// @brief Returns an owning copy of the requested catalog value, or its lookup error.
		[[nodiscard]] auto sceneName(SceneHandle scene) const { return copyOf(sceneField(catalog_, scene, &AssetScene::name)); }
		[[nodiscard]] auto sceneNodes(SceneHandle scene) const -> std::expected<Vector<NodeHandle>, Error>;
		/// @brief Returns an owning copy of the requested catalog value, or its lookup error.
		[[nodiscard]] auto sceneMeshes(SceneHandle scene) const { return copyOf(sceneField(catalog_, scene, &AssetScene::meshes)); }
		/// @brief Returns an owning copy of the requested catalog value, or its lookup error.
		[[nodiscard]] auto sceneMaterials(SceneHandle scene) const { return copyOf(sceneField(catalog_, scene, &AssetScene::materials)); }
		/// @brief Returns an owning copy of the requested catalog value, or its lookup error.
		[[nodiscard]] auto sceneLights(SceneHandle scene) const { return copyOf(sceneField(catalog_, scene, &AssetScene::lights)); }
		/// @brief Returns an owning copy of the requested catalog value, or its lookup error.
		[[nodiscard]] auto sceneCameras(SceneHandle scene) const { return copyOf(sceneField(catalog_, scene, &AssetScene::cameras)); }
		[[nodiscard]] auto sceneNodeCount(SceneHandle scene) const -> std::expected<std::size_t, Error>;
		/// @brief Counts scene meshes without copying the handle list.
		[[nodiscard]] auto sceneMeshCount(SceneHandle scene) const { return sizeOf(sceneField(catalog_, scene, &AssetScene::meshes)); }
		/// @brief Counts scene materials without copying the handle list.
		[[nodiscard]] auto sceneMaterialCount(SceneHandle scene) const { return sizeOf(sceneField(catalog_, scene, &AssetScene::materials)); }
		[[nodiscard]] auto sceneTextureCount(SceneHandle scene) const -> std::expected<std::size_t, Error>;
		/// @brief Counts scene lights without copying the handle list.
		[[nodiscard]] auto sceneLightCount(SceneHandle scene) const { return sizeOf(sceneField(catalog_, scene, &AssetScene::lights)); }
		/// @brief Counts scene cameras without copying the handle list.
		[[nodiscard]] auto sceneCameraCount(SceneHandle scene) const { return sizeOf(sceneField(catalog_, scene, &AssetScene::cameras)); }
		/// @brief Reads the root handle without copying the tree; absent roots return missing_object.
		[[nodiscard]] auto sceneRootNode(SceneHandle scene) const -> std::expected<NodeHandle, Error> {
			const auto tree = sceneField(catalog_, scene, &AssetScene::tree);
			if (!tree) { return std::unexpected(tree.error()); }
			if (!(*tree)->root.valid()) { return std::unexpected(Error::missing_object); }
			return (*tree)->root;
		}
		[[nodiscard]] auto sceneNodeChildren(SceneHandle scene, NodeHandle node) const -> std::expected<Vector<NodeHandle>, Error> {
			const auto source = sceneWithNode(catalog_, scene, node);
			if (!source) { return std::unexpected(source.error()); }
			return (*source)->tree.children(node);
		}
		[[nodiscard]] auto sceneNodeParent(SceneHandle scene, NodeHandle node) const -> std::expected<std::optional<NodeHandle>, Error> {
			const auto source = sceneWithNode(catalog_, scene, node);
			if (!source) { return std::unexpected(source.error()); }
			return (*source)->tree.parent(node);
		}
		/// @brief Returns an owning copy of the requested catalog value, or its lookup error.
		[[nodiscard]] auto lightData(LightHandle light) const { return copyOf(field(catalog_.lights, light, &Light::data)); }
		/// @brief Returns an owning copy of the requested catalog value, or its lookup error.
		[[nodiscard]] auto cameraData(CameraHandle camera) const { return copyOf(field(catalog_.cameras, camera, &CameraAsset::data)); }
		/// @brief Returns an owning copy of the requested catalog value, or its lookup error.
		[[nodiscard]] auto nodeName(NodeHandle node) const { return copyOf(field(catalog_.nodes, node, &Node::name)); }
		/// @brief Returns an owning copy of the requested catalog value, or its lookup error.
		[[nodiscard]] auto nodeTransform(NodeHandle node) const { return copyOf(field(catalog_.nodes, node, &Node::transform)); }
		/// @brief Returns an owning copy of the requested catalog value, or its lookup error.
		[[nodiscard]] auto nodeMeshes(NodeHandle node) const { return copyOf(field(catalog_.nodes, node, &Node::meshes)); }
		[[nodiscard]] auto nodeMaterials(NodeHandle node) const -> std::expected<Vector<MaterialHandle>, Error>;
		/// @brief Returns an owning copy of the requested catalog value, or its lookup error.
		[[nodiscard]] auto meshName(MeshHandle mesh) const { return copyOf(field(catalog_.meshes, mesh, &AssetMesh::name)); }
		[[nodiscard]] auto meshVertexCount(MeshHandle mesh) const -> std::expected<VertexCount, Error>;
		[[nodiscard]] auto meshIndexCount(MeshHandle mesh) const -> std::expected<IndexCount, Error>;
		/// @brief Returns an owning copy of the requested catalog value, or its lookup error.
		[[nodiscard]] auto meshMaterial(MeshHandle mesh) const { return copyOf(field(catalog_.meshes, mesh, &AssetMesh::material)); }
		/// @brief Returns an owning copy of the requested catalog value, or its lookup error.
		[[nodiscard]] auto meshBounds(MeshHandle mesh) const { return copyOf(field(catalog_.meshes, mesh, &AssetMesh::bounds)); }
		[[nodiscard]] auto meshPositions(MeshHandle mesh) const -> std::expected<Vector<Vec3>, Error>;
		[[nodiscard]] auto meshNormals(MeshHandle mesh) const -> std::expected<Vector<Vec3>, Error>;
		[[nodiscard]] auto meshTexcoords(MeshHandle mesh) const -> std::expected<Vector<Vec2>, Error>;
		[[nodiscard]] auto meshTangents(MeshHandle mesh) const -> std::expected<Vector<Vec4>, Error>;
		/// @brief Returns an owning copy of the requested catalog value, or its lookup error.
		[[nodiscard]] auto meshIndices(MeshHandle mesh) const { return copyOf(field(catalog_.meshes, mesh, &AssetMesh::indices)); }
		/// @brief Returns an owning copy of the requested catalog value, or its lookup error.
		[[nodiscard]] auto materialName(MaterialHandle material) const { return copyOf(field(catalog_.materials, material, &Material::name)); }
		/// @brief Returns an owning copy of the requested catalog value, or its lookup error.
		[[nodiscard]] auto materialBaseColor(MaterialHandle material) const { return copyOf(field(catalog_.materials, material, &Material::base_color)); }
		/// @brief Returns an owning copy of the requested catalog value, or its lookup error.
		[[nodiscard]] auto materialTextureSources(MaterialHandle material) const { return copyOf(field(catalog_.materials, material, &Material::texture_sources)); }
		/// @brief Returns an owning copy of the requested catalog value, or its lookup error.
		[[nodiscard]] auto materialFactors(MaterialHandle material) const { return copyOf(field(catalog_.materials, material, &Material::factors)); }

		[[nodiscard]] auto meshGeometry(MeshHandle mesh) const -> std::expected<std::shared_ptr<const std::vector<RenderVertex>>, Error>;
		[[nodiscard]] auto meshPositionsView(MeshHandle mesh) const -> std::expected<std::ranges::transform_view<std::span<const RenderVertex>, Vec3 RenderVertex::*>, Error>;
		[[nodiscard]] auto meshNormalsView(MeshHandle mesh) const -> std::expected<std::ranges::transform_view<std::span<const RenderVertex>, Vec3 RenderVertex::*>, Error>;
		[[nodiscard]] auto meshTexcoordsView(MeshHandle mesh) const -> std::expected<std::ranges::transform_view<std::span<const RenderVertex>, Vec2 RenderVertex::*>, Error>;
		[[nodiscard]] auto meshTangentsView(MeshHandle mesh) const -> std::expected<std::ranges::transform_view<std::span<const RenderVertex>, Vec4 RenderVertex::*>, Error>;
		[[nodiscard]] auto meshIndicesView(MeshHandle mesh) const -> std::expected<std::span<const std::uint32_t>, Error>;

	private:
		/// @brief Texture slots read from imported materials; for two slots with the same semantic the earlier one wins.
		inline static constexpr std::array texture_types{aiTextureType_BASE_COLOR, aiTextureType_DIFFUSE,
																			aiTextureType_NORMALS, aiTextureType_HEIGHT,
																			aiTextureType_DIFFUSE_ROUGHNESS, aiTextureType_METALNESS,
																			aiTextureType_EMISSIVE, aiTextureType_AMBIENT_OCCLUSION,
																			aiTextureType_LIGHTMAP};

		[[nodiscard]] static auto normalized(std::filesystem::path path)									-> std::filesystem::path;
		[[nodiscard]] static auto vec3(const aiVector3D &v)													-> Vec3;
		[[nodiscard]] static auto quat(const aiQuaternion &q)													-> Quat;
		[[nodiscard]] static auto transform(const aiMatrix4x4 &matrix)										-> Transform;
		[[nodiscard]] static auto color(const aiColor3D &value)												-> LinearColor;
		[[nodiscard]] static auto color(const aiColor4D &value)												-> LinearColor;
		[[nodiscard]] static auto textureSemantic(aiTextureType type)									-> MaterialTextureSemantic;
		[[nodiscard]] static auto globalTransform(const aiScene &scene, const aiString &node_name)	-> aiMatrix4x4;
		[[nodiscard]] static auto lightRange(const aiLight &source, const aiNode *node)					-> LightRange;
		[[nodiscard]] static auto lightDescriptor(const aiScene &scene, const aiLight &source)			-> std::optional<LightDescriptor>;
		[[nodiscard]] static auto cameraAspect(const aiCamera &source)										-> Scalar;
		[[nodiscard]] static auto cameraFovY(const aiCamera &source)										-> FovY;
		[[nodiscard]] static auto cameraDescriptor(const aiScene &scene, const aiCamera &source)		-> CameraDescriptor;
		[[nodiscard]] static auto textureSource(const aiScene &scene, const aiString &path, MaterialTextureSemantic semantic,
															 const std::filesystem::path &model_path)				-> std::optional<MaterialTextureSource>;
		[[nodiscard]] static auto name(const aiString &text, std::string fallback)						-> std::string;
		[[nodiscard]] static std::filesystem::path texturePath(const aiString &path,
																					const std::filesystem::path &scene_dir);
		template <typename T>
		[[nodiscard]] static std::expected<const T *, Error> require(const Table<T> &table,
																							typename T::Handle handle);
		template <typename T>
		[[nodiscard]] static std::expected<const T *, Error> sceneField(const Catalog &catalog, SceneHandle handle,
																					T AssetScene::*field);
		template <typename Descriptor, typename T>
		[[nodiscard]] static std::expected<const T *, Error> field(const Table<Descriptor> &table,
																			typename Descriptor::Handle handle,
																			T Descriptor::*member);
		template <typename T>
		[[nodiscard]] static std::expected<T, Error> copyOf(std::expected<const T *, Error> value);
		template <typename T>
		[[nodiscard]] static std::expected<std::size_t, Error>
		sizeOf(std::expected<const Vector<T> *, Error> value);
		[[nodiscard]] static std::expected<const AssetScene *, Error> sceneWithNode(const Catalog &catalog,
																										SceneHandle scene,
																										NodeHandle node);
		[[nodiscard]] static std::expected<Vector<MaterialHandle>, Error> materials(Catalog &catalog, const aiScene &scene,
																									const std::filesystem::path &model_path);
		[[nodiscard]] static auto boundsOf(const aiMesh &source)												-> Bounds;
		[[nodiscard]] static auto indexCount(const aiMesh &source)											-> std::uint64_t;
		[[nodiscard]] static std::expected<Vector<MeshHandle>, Error>
		meshes(Catalog &catalog, const aiScene &scene, const Vector<MaterialHandle> &material_handles);
		[[nodiscard]] static std::expected<NodeHandle, Error> node(Catalog &catalog,
																						const aiNode &source,
																						const Vector<MeshHandle> &mesh_handles,
																						AssetScene &scene, NodeHandle parent = {});
		[[nodiscard]] static auto lights(Catalog &catalog, const aiScene &scene)						-> std::expected<Vector<LightHandle>, Error>;
		[[nodiscard]] static auto cameras(Catalog &catalog, const aiScene &scene)						-> std::expected<Vector<CameraHandle>, Error>;
		[[nodiscard]] static std::expected<SceneHandle, Error> import(Catalog &catalog, const aiScene &source,
																							const std::filesystem::path &path);

		Catalog catalog_{};																						///< All descriptors loaded through this asset system.
	};

} // namespace vve::simple

namespace vve::simple {

		std::filesystem::path AssetSystem::normalized(std::filesystem::path path) {			///< Canonical path if possible.
			std::error_code error{};
			const auto canonical = std::filesystem::weakly_canonical(path, error);
			return error ? path.lexically_normal() : canonical;
		}

		Vec3 AssetSystem::vec3(const aiVector3D &v) { return Vec3(v.x, v.y, v.z); }			///< Assimp vector conversion.
		Quat AssetSystem::quat(const aiQuaternion &q) { return Quat(q.w, q.x, q.y, q.z); }	///< Quaternion conversion.

		Transform AssetSystem::transform(const aiMatrix4x4 &matrix) {								///< Converts Assimp local transforms.
			aiVector3D scale{};
			aiQuaternion rotation{};
			aiVector3D translation{};
			matrix.Decompose(scale, rotation, translation);
			return Transform{.translation = Position{.value = vec3(translation)},
									.rotation = Rotation{.value = quat(rotation)},
									.scale = Scale{.value = vec3(scale)}};
		}

		LinearColor AssetSystem::color(const aiColor3D &value) {									///< Converts Assimp RGB colors.
			return LinearColor{.value = Vec3(value.r, value.g, value.b)};
		}
		LinearColor AssetSystem::color(const aiColor4D &value) {									///< Converts Assimp RGBA colors.
			return LinearColor{.value = Vec3(value.r, value.g, value.b)};
		}

		MaterialTextureSemantic AssetSystem::textureSemantic(aiTextureType type) {		///< Maps Assimp slots without exposing Assimp to rendering.
			switch (type) {
			case aiTextureType_NORMALS: return MaterialTextureSemantic::normal;
			case aiTextureType_HEIGHT: return MaterialTextureSemantic::height;
			case aiTextureType_METALNESS: return MaterialTextureSemantic::metalness;
			case aiTextureType_DIFFUSE_ROUGHNESS: return MaterialTextureSemantic::roughness;
			case aiTextureType_EMISSIVE: return MaterialTextureSemantic::emissive;
			case aiTextureType_AMBIENT_OCCLUSION:
			case aiTextureType_LIGHTMAP: return MaterialTextureSemantic::ambient_occlusion;
			default: return MaterialTextureSemantic::base_color;
			}
		}

		/// @brief Returns the world matrix of the node with this name; lights and cameras are defined relative to it.
		aiMatrix4x4 AssetSystem::globalTransform(const aiScene &scene, const aiString &node_name) {
			const aiNode *node = scene.mRootNode != nullptr ? scene.mRootNode->FindNode(node_name) : nullptr;
			aiMatrix4x4 world{};
			for (; node != nullptr; node = node->mParent) { world = node->mTransformation * world; }
			return world;
		}

		/// @brief Derives the range after which the renderer fades a point or spot light out.
		///
		/// glTF stores an explicit range in the node metadata. Otherwise the attenuation c + l*d + q*d^2 is used,
		/// with the range where the light has dropped to 1%. Assimp's default (l = 1) and glTF's plain inverse square
		/// (q = 1) carry no range, so they get the engine default.
		LightRange AssetSystem::lightRange(const aiLight &source, const aiNode *node) {
			if (float range{}; node != nullptr && node->mMetaData != nullptr && node->mMetaData->Get("PBR_LightRange", range) &&
					std::isfinite(range) && range > 0.0F) {
				return LightRange{.value = static_cast<Scalar>(range)};
			}
			const double constant{source.mAttenuationConstant};
			const double linear{source.mAttenuationLinear};
			const double quadratic{source.mAttenuationQuadratic};
			const bool assimp_default = constant == 0.0 && linear == 1.0 && quadratic == 0.0;
			const bool inverse_square = constant == 0.0 && linear == 0.0 && quadratic == 1.0;
			if (!assimp_default && !inverse_square) {
				constexpr double one_percent{100.0}; // Denominator at which the light is down to 1%.
				double range{};
				if (quadratic > 0.0) {
					range = (-linear + std::sqrt(linear * linear + 4.0 * quadratic * (one_percent - constant))) / (2.0 * quadratic);
				} else if (linear > 0.0) {
					range = (one_percent - constant) / linear;
				}
				if (std::isfinite(range) && range > 0.0) { return LightRange{.value = static_cast<Scalar>(range)}; }
			}
			return {};
		}

		std::optional<LightDescriptor> AssetSystem::lightDescriptor(const aiScene &scene, const aiLight &source) {	///< Converts supported Assimp lights to world space.
			const aiNode *node = scene.mRootNode != nullptr ? scene.mRootNode->FindNode(source.mName) : nullptr;
			const aiMatrix4x4 world = globalTransform(scene, source.mName);
			const aiVector3D position = world * source.mPosition;
			aiVector3D direction = aiMatrix3x3{world} * source.mDirection;
			if (direction.SquareLength() > 0.0F) { direction.Normalize(); }
			LightDescriptor data{.color = color(source.mColorDiffuse),
										.intensity = LightIntensity{.value = one()},
										.direction = Direction{.value = vec3(direction)},
										.position = Position{.value = vec3(position)},
										.range = lightRange(source, node),
										.cone = SpotConeAngle{.radians = static_cast<Scalar>(source.mAngleOuterCone)},
										.inner_cone = SpotConeAngle{.radians = static_cast<Scalar>(source.mAngleInnerCone)}};
			switch (source.mType) {
			case aiLightSource_DIRECTIONAL: data.kind = LightKind::directional; return data;
			case aiLightSource_POINT: data.kind = LightKind::point; return data;
			case aiLightSource_SPOT: data.kind = LightKind::spot; return data;
			default: return std::nullopt;
			}
		}

		Scalar AssetSystem::cameraAspect(const aiCamera &source) {								///< Returns a finite projection aspect ratio.
			return source.mAspect > 0.0F && std::isfinite(source.mAspect) ? static_cast<Scalar>(source.mAspect) : one();
		}

		FovY AssetSystem::cameraFovY(const aiCamera &source) {									///< Converts Assimp horizontal FOV to facade vertical FOV.
			const auto horizontal = source.mHorizontalFOV > 0.0F && std::isfinite(source.mHorizontalFOV)
												 ? static_cast<Scalar>(source.mHorizontalFOV)
												 : FovY{}.radians;
			const auto aspect = cameraAspect(source);
			const auto half = horizontal / static_cast<Scalar>(2);
			return FovY{.radians = static_cast<Scalar>(2) * static_cast<Scalar>(std::atan(std::tan(half) / aspect))};
		}

		CameraDescriptor AssetSystem::cameraDescriptor(const aiScene &scene, const aiCamera &source) {	///< Converts Assimp camera data to world space.
			const aiMatrix4x4 world = globalTransform(scene, source.mName);
			const aiMatrix3x3 rotation{world};
			aiVector3D look_at = rotation * source.mLookAt;
			aiVector3D up = rotation * source.mUp;
			if (look_at.SquareLength() > 0.0F) { look_at.Normalize(); }
			if (up.SquareLength() > 0.0F) { up.Normalize(); }
			return CameraDescriptor{.position = Position{.value = vec3(world * source.mPosition)},
											.direction = Direction{.value = vec3(look_at)},
											.up = Direction{.value = vec3(up)},
											.fov = cameraFovY(source),
											.aspect = cameraAspect(source),
											.near_clip = static_cast<Scalar>(source.mClipPlaneNear),
											.far_clip = static_cast<Scalar>(source.mClipPlaneFar)};
		}

		std::string AssetSystem::name(const aiString &text, std::string fallback) {			///< Name or generated fallback.
			return text.length > 0 ? std::string{text.C_Str()} : std::move(fallback);
		}

		std::filesystem::path AssetSystem::texturePath(const aiString &path,
															const std::filesystem::path &scene_dir) {
			auto result = std::filesystem::path(path.C_Str());
			if (result.empty() || result.string().starts_with('*')) { return result; }
			return normalized(result.is_absolute() ? result : scene_dir / result);
		}

		/// @brief Resolves one material texture to a file path or to image data embedded in the model file.
		std::optional<MaterialTextureSource> AssetSystem::textureSource(const aiScene &scene, const aiString &path,
			MaterialTextureSemantic semantic, const std::filesystem::path &model_path) {
			if (const aiTexture *embedded = scene.GetEmbeddedTexture(path.C_Str()); embedded != nullptr) {
				auto image = std::make_shared<EmbeddedImage>();
				if (embedded->mHeight == 0U) {
					// Compressed file (PNG, JPEG, ...): mWidth is the byte count.
					const auto *begin = reinterpret_cast<const std::byte *>(embedded->pcData);
					image->bytes.assign(begin, begin + embedded->mWidth);
				} else {
					image->extent = PixelExtent{.width = embedded->mWidth, .height = embedded->mHeight};
					image->bytes.reserve(static_cast<std::size_t>(embedded->mWidth) * embedded->mHeight * 4U);
					for (std::size_t texel{}; texel < static_cast<std::size_t>(embedded->mWidth) * embedded->mHeight; ++texel) {
						const aiTexel &value = embedded->pcData[texel]; // Stored as BGRA.
						image->bytes.insert(image->bytes.end(), {std::byte{value.r}, std::byte{value.g}, std::byte{value.b}, std::byte{value.a}});
					}
				}
				// Embedded images have no file; the model path plus the texture reference is a unique key.
				return MaterialTextureSource{.semantic = semantic,
					.path = std::filesystem::path{model_path.string() + "#" + path.C_Str()}, .embedded = std::move(image)};
			}
			const auto source_path = texturePath(path, model_path.parent_path());
			if (!source_path.is_absolute()) { return std::nullopt; } // A '*' reference without embedded data.
			return MaterialTextureSource{.semantic = semantic, .path = source_path};
		}

		template <typename T>
		auto AssetSystem::require(const Table<T> &table, typename T::Handle handle)					-> std::expected<const T *, Error>{
			const auto *value = table.find(handle);
			if (value == nullptr) { return std::unexpected(Error::missing_object); }
			return value;
		}

		/// @brief Borrows a scene member until catalog mutation or destruction; propagates lookup errors.
		template <typename T>
		auto AssetSystem::sceneField(const Catalog &catalog, SceneHandle handle, T AssetScene::*member) -> std::expected<const T *, Error> {
			return field(catalog.scenes, handle, member);
		}

		/// @brief Borrows a catalog member until catalog mutation or destruction; propagates lookup errors.
		template <typename Descriptor, typename T>
		std::expected<const T *, Error> AssetSystem::field(const Table<Descriptor> &table,
																	typename Descriptor::Handle handle, T Descriptor::*member) {
			const auto descriptor = require(table, handle);
			if (!descriptor) { return std::unexpected(descriptor.error()); }
			return std::addressof((*descriptor)->*member);
		}

		/// @brief Copies a borrowed value once into an owning query result, preserving lookup errors.
		template <typename T>
		std::expected<T, Error> AssetSystem::copyOf(std::expected<const T *, Error> value) {
			if (!value) { return std::unexpected(value.error()); }
			return **value;
		}

		/// @brief Reads a borrowed vector's size without copying its elements, preserving lookup errors.
		template <typename T>
		auto AssetSystem::sizeOf(std::expected<const Vector<T> *, Error> value)									-> std::expected<std::size_t, Error>{
			if (!value) { return std::unexpected(value.error()); }
			return (*value)->size();
		}

		std::expected<const AssetScene *, Error> AssetSystem::sceneWithNode(const Catalog &catalog, SceneHandle scene,
																							NodeHandle node) {
			const auto value = require(catalog.scenes, scene);
			if (!value) { return std::unexpected(value.error()); }
			if (!(*value)->tree.contains(node)) { return std::unexpected(Error::missing_object); }
			return *value;
		}

		/// @brief Imports material factors and typed sources, returning handles in Assimp material order.
		std::expected<Vector<MaterialHandle>, Error>
		AssetSystem::materials(Catalog &catalog, const aiScene &scene, const std::filesystem::path &model_path) {
			Vector<MaterialHandle> result(scene.mNumMaterials);
			for (unsigned i = 0; i < scene.mNumMaterials; ++i) {
				const auto *source = scene.mMaterials[i];
				aiString material_name{};
				if (source != nullptr) { source->Get(AI_MATKEY_NAME, material_name); }

				auto item = Material{.handle = makeCounterHandle<MaterialHandle>(),
										.name = ObjectName{.value = name(material_name, "Material_" + std::to_string(i))}};
				if (source != nullptr) {
					aiColor4D base_color{};
					if (source->Get(AI_MATKEY_BASE_COLOR, base_color) == AI_SUCCESS ||
							source->Get(AI_MATKEY_COLOR_DIFFUSE, base_color) == AI_SUCCESS) {
						item.base_color = color(base_color);
					}
					if (float roughness{}; source->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) == AI_SUCCESS) {
						item.factors.roughness = static_cast<Scalar>(roughness);
					}
					if (float metalness{}; source->Get(AI_MATKEY_METALLIC_FACTOR, metalness) == AI_SUCCESS) {
						item.factors.metalness = static_cast<Scalar>(metalness);
					}
					if (aiColor3D emissive{}; source->Get(AI_MATKEY_COLOR_EMISSIVE, emissive) == AI_SUCCESS) {
						item.factors.emissive = color(emissive);
					}
					// Only the first texture of a slot is rendered, and each semantic is used once: glTF lists its
					// base color as DIFFUSE and BASE_COLOR, and its metallic-roughness map as METALNESS and DIFFUSE_ROUGHNESS.
					for (const auto type : texture_types) {
						aiString path{};
						if (source->GetTextureCount(type) == 0U || source->GetTexture(type, 0U, &path) != AI_SUCCESS) { continue; }
						if (type == aiTextureType_HEIGHT && source->GetTextureCount(aiTextureType_NORMALS) > 0U) { continue; }
						const auto semantic = textureSemantic(type);
						if (std::ranges::any_of(item.texture_sources, [semantic](const MaterialTextureSource &existing) {
								return existing.semantic == semantic; })) { continue; }
						auto texture_source = textureSource(scene, path, semantic, model_path);
						if (!texture_source) { continue; }
						item.texture_sources.push_back(std::move(*texture_source));
					}
				}
				result[i] = item.handle; // Save identity before transferring the descriptor's arrays.
				if (auto added = catalog.materials.add(std::move(item)); !added) { return std::unexpected(added.error()); }
			}
			return result;
		}

		Bounds AssetSystem::boundsOf(const aiMesh &source) {											///< Computes object-space bounds from source vertices.
			Bounds bounds{};
			if (source.mNumVertices == 0 || source.mVertices == nullptr) { return bounds; }
			bounds.valid = true;
			bounds.minimum.value = vec3(source.mVertices[0]);
			bounds.maximum.value = bounds.minimum.value;
			for (unsigned i = 1; i < source.mNumVertices; ++i) {
				bounds.minimum.value = math::min(bounds.minimum.value, vec3(source.mVertices[i]));
				bounds.maximum.value = math::max(bounds.maximum.value, vec3(source.mVertices[i]));
			}
			return bounds;
		}

		std::uint64_t AssetSystem::indexCount(const aiMesh &source) {								///< Counts all indices in all faces.
			std::uint64_t result = 0;
			for (unsigned face = 0; face < source.mNumFaces; ++face) { result += source.mFaces[face].mNumIndices; }
			return result;
		}

		/// @brief Imports each mesh once and moves its arrays into the catalog, retaining its handle.
		std::expected<Vector<MeshHandle>, Error>
		AssetSystem::meshes(Catalog &catalog, const aiScene &scene, const Vector<MaterialHandle> &material_handles) {
			Vector<MeshHandle> result(scene.mNumMeshes);
			for (unsigned i = 0; i < scene.mNumMeshes; ++i) {
				const auto *source = scene.mMeshes[i];
				if (source == nullptr) { continue; }
				const auto material = source->mMaterialIndex < material_handles.size()
													? material_handles[source->mMaterialIndex]
													: MaterialHandle{};
				auto item = AssetMesh{.handle = makeCounterHandle<MeshHandle>(),
										.name = ObjectName{.value = name(source->mName, "Mesh_" + std::to_string(i))},
										.material = material,
										.bounds = boundsOf(*source)};
				auto vertices = std::make_shared<std::vector<RenderVertex>>();
				vertices->reserve(source->mNumVertices);
				// Build the final shared payload once; absent tangents stay zero, preserving empty tangent queries.
				for (unsigned vertex = 0; vertex < source->mNumVertices; ++vertex) {
					const auto uv = source->HasTextureCoords(0) ? source->mTextureCoords[0][vertex] : aiVector3D{};
					auto value = RenderVertex{.position = source->mVertices != nullptr ? vec3(source->mVertices[vertex]) : zeroVec3(),
						.normal = source->HasNormals() ? vec3(source->mNormals[vertex]) : zeroVec3(), .uv = Vec2{uv.x, uv.y}};
					if (source->HasTangentsAndBitangents()) {
						const auto tangent = vec3(source->mTangents[vertex]);
						value.tangent = Vec4{tangent.x, tangent.y, tangent.z,
							dot(cross(value.normal, tangent), vec3(source->mBitangents[vertex])) < zero() ? -one() : one()};
					}
					vertices->push_back(value);
				}
				item.vertices = std::move(vertices);
				item.indices.reserve(static_cast<std::size_t>(indexCount(*source)));
				for (unsigned face = 0; face < source->mNumFaces; ++face) {
					const auto &source_face = source->mFaces[face];
					if (source_face.mNumIndices != 3U) { continue; } // Points and lines left by triangulation would break the triangle list.
					for (unsigned index = 0; index < source_face.mNumIndices; ++index) {
						item.indices.push_back(source_face.mIndices[index]);
					}
				}
				result[i] = item.handle; // Save identity before transferring the descriptor's arrays.
				if (auto added = catalog.meshes.add(std::move(item)); !added) { return std::unexpected(added.error()); }
			}
			return result;
		}

		std::expected<NodeHandle, Error>
		AssetSystem::node(Catalog &catalog, const aiNode &source,
								const Vector<MeshHandle> &mesh_handles, AssetScene &scene, NodeHandle parent) {
			auto item = Node{.handle = makeCounterHandle<NodeHandle>(),
									.name = ObjectName{.value = name(source.mName, "Node_" + std::to_string(scene.tree.nodes().size()))},
									.transform = transform(source.mTransformation)};
			for (unsigned slot = 0; slot < source.mNumMeshes; ++slot) {
				const auto mesh_index = source.mMeshes[slot];
				if (mesh_index >= mesh_handles.size() || !mesh_handles[mesh_index].valid()) { continue; }
				item.meshes.push_back(mesh_handles[mesh_index]);
			}

			const auto handle = item.handle;
			if (auto added = catalog.nodes.add(std::move(item)); !added) { return std::unexpected(added.error()); }
			const auto linked = parent.valid() ? scene.tree.addChild(parent, handle) : scene.tree.setRoot(handle);
			if (!linked) { return std::unexpected(linked.error()); }

			for (unsigned i = 0; i < source.mNumChildren; ++i) {
				if (source.mChildren[i] == nullptr) { continue; }
				if (auto child = node(catalog, *source.mChildren[i], mesh_handles, scene, handle); !child) {
					return std::unexpected(child.error());
				}
			}
			return handle;
		}

		auto AssetSystem::lights(Catalog &catalog, const aiScene &scene)								-> std::expected<Vector<LightHandle>, Error>{
			Vector<LightHandle> result{};
			result.reserve(scene.mNumLights);
			for (unsigned i = 0; i < scene.mNumLights; ++i) {
				const auto *source = scene.mLights[i];
				if (source == nullptr) { continue; }
				auto data = lightDescriptor(scene, *source);
				if (!data) { continue; }
				auto item = Light{.handle = makeCounterHandle<LightHandle>(), .data = *data};
				const auto handle = item.handle;
				if (auto added = catalog.lights.add(std::move(item)); !added) { return std::unexpected(added.error()); }
				result.push_back(handle);
			}
			return result;
		}

		auto AssetSystem::cameras(Catalog &catalog, const aiScene &scene)								-> std::expected<Vector<CameraHandle>, Error>{
			Vector<CameraHandle> result{};
			result.reserve(scene.mNumCameras);
			for (unsigned i = 0; i < scene.mNumCameras; ++i) {
				const auto *source = scene.mCameras[i];
				if (source == nullptr) { continue; }
				auto item = CameraAsset{.handle = makeCounterHandle<CameraHandle>(), .data = cameraDescriptor(scene, *source)};
				const auto handle = item.handle;
				if (auto added = catalog.cameras.add(std::move(item)); !added) { return std::unexpected(added.error()); }
				result.push_back(handle);
			}
			return result;
		}

		std::expected<SceneHandle, Error>
		AssetSystem::import(Catalog &catalog, const aiScene &source, const std::filesystem::path &path) {
			const auto imported_materials = materials(catalog, source, path);
			if (!imported_materials) { return std::unexpected(imported_materials.error()); }
			const auto imported_meshes = meshes(catalog, source, *imported_materials);
			if (!imported_meshes) { return std::unexpected(imported_meshes.error()); }
			const auto imported_lights = lights(catalog, source);
			if (!imported_lights) { return std::unexpected(imported_lights.error()); }
			const auto imported_cameras = cameras(catalog, source);
			if (!imported_cameras) { return std::unexpected(imported_cameras.error()); }

			auto scene = AssetScene{.handle = makeCounterHandle<SceneHandle>(),
										.name = ObjectName{.value = path.filename().string()},
										.meshes = *imported_meshes,
										.materials = *imported_materials,
										.lights = *imported_lights,
										.cameras = *imported_cameras};
			if (source.mRootNode != nullptr) {
				if (auto root = node(catalog, *source.mRootNode, *imported_meshes, scene); !root) {
					return std::unexpected(root.error());
				}
			}
			const auto handle = scene.handle;
			if (auto added = catalog.scenes.add(std::move(scene)); !added) { return std::unexpected(added.error()); }
			return handle;
		}

	/// @brief Returns the tree's creation-order handles, which follow the imported scene's pre-order traversal.
	auto AssetSystem::sceneNodes(SceneHandle scene) const -> std::expected<Vector<NodeHandle>, Error> {
		const auto source = require(catalog_.scenes, scene);
		if (!source) { return std::unexpected(source.error()); }
		return (*source)->tree.nodes();
	}

	/// @brief Resolves one material per attached mesh, preserving mesh order and repeated handles.
	auto AssetSystem::nodeMaterials(NodeHandle node) const -> std::expected<Vector<MaterialHandle>, Error> {
		const auto meshes = field(catalog_.nodes, node, &Node::meshes);
		if (!meshes) { return std::unexpected(meshes.error()); }
		Vector<MaterialHandle> materials{};
		materials.reserve((*meshes)->size());
		// Each mesh owns its material association; nodes retain only the mesh handles.
		for (const auto mesh : **meshes) {
			const auto material = meshMaterial(mesh);
			if (!material) { return std::unexpected(material.error()); }
			materials.push_back(*material);
		}
		return materials;
	}

	/// @brief Counts nodes through the borrowed scene tree without copying its topology or order vector.
	auto AssetSystem::sceneNodeCount(SceneHandle scene) const -> std::expected<std::size_t, Error> {
		return sceneField(catalog_, scene, &AssetScene::tree).transform([](const auto *tree) { return tree->nodes().size(); });
	}

	/// @brief Shares immutable catalog geometry with render meshes; unknown handles return missing_object.
	auto AssetSystem::meshGeometry(MeshHandle mesh) const -> std::expected<std::shared_ptr<const std::vector<RenderVertex>>, Error> {
		return copyOf(field(catalog_.meshes, mesh, &AssetMesh::vertices));
	}

	/// @brief Borrows Positions from interleaved vertices; valid until catalog mutation or destruction.
	auto AssetSystem::meshPositionsView(MeshHandle mesh) const -> std::expected<std::ranges::transform_view<std::span<const RenderVertex>, Vec3 RenderVertex::*>, Error> {
		return field(catalog_.meshes, mesh, &AssetMesh::vertices).transform([](const auto *values) {
			return std::span<const RenderVertex>{**values} | std::views::transform(&RenderVertex::position);
		});
	}

	/// @brief Extracts an owning attribute array from the shared vertex buffer.
	auto AssetSystem::meshPositions(MeshHandle mesh) const -> std::expected<Vector<Vec3>, Error> {
		return meshPositionsView(mesh).transform([](auto values) { return Vector<Vec3>{values.begin(), values.end()}; });
	}

	/// @brief Borrows Normals from interleaved vertices; valid until catalog mutation or destruction.
	auto AssetSystem::meshNormalsView(MeshHandle mesh) const -> std::expected<std::ranges::transform_view<std::span<const RenderVertex>, Vec3 RenderVertex::*>, Error> {
		return field(catalog_.meshes, mesh, &AssetMesh::vertices).transform([](const auto *values) {
			return std::span<const RenderVertex>{**values} | std::views::transform(&RenderVertex::normal);
		});
	}

	/// @brief Extracts an owning attribute array from the shared vertex buffer.
	auto AssetSystem::meshNormals(MeshHandle mesh) const -> std::expected<Vector<Vec3>, Error> {
		return meshNormalsView(mesh).transform([](auto values) { return Vector<Vec3>{values.begin(), values.end()}; });
	}

	/// @brief Borrows Texcoords from interleaved vertices; valid until catalog mutation or destruction.
	auto AssetSystem::meshTexcoordsView(MeshHandle mesh) const -> std::expected<std::ranges::transform_view<std::span<const RenderVertex>, Vec2 RenderVertex::*>, Error> {
		return field(catalog_.meshes, mesh, &AssetMesh::vertices).transform([](const auto *values) {
			return std::span<const RenderVertex>{**values} | std::views::transform(&RenderVertex::uv);
		});
	}

	/// @brief Extracts an owning attribute array from the shared vertex buffer.
	auto AssetSystem::meshTexcoords(MeshHandle mesh) const -> std::expected<Vector<Vec2>, Error> {
		return meshTexcoordsView(mesh).transform([](auto values) { return Vector<Vec2>{values.begin(), values.end()}; });
	}

	/// @brief Borrows Tangents from interleaved vertices; valid until catalog mutation or destruction.
	auto AssetSystem::meshTangentsView(MeshHandle mesh) const -> std::expected<std::ranges::transform_view<std::span<const RenderVertex>, Vec4 RenderVertex::*>, Error> {
		return field(catalog_.meshes, mesh, &AssetMesh::vertices).transform([](const auto *values) {
			return (std::span<const RenderVertex>{**values}).first((**values).empty() || (**values).front().tangent.w == zero() ? 0U : (**values).size()) | std::views::transform(&RenderVertex::tangent);
		});
	}

	/// @brief Extracts an owning attribute array from the shared vertex buffer.
	auto AssetSystem::meshTangents(MeshHandle mesh) const -> std::expected<Vector<Vec4>, Error> {
		return meshTangentsView(mesh).transform([](auto values) { return Vector<Vec4>{values.begin(), values.end()}; });
	}

	/// @brief Borrows mesh indices; valid only until catalog mutation or destruction. Unknown handles return missing_object.
	auto AssetSystem::meshIndicesView(MeshHandle mesh) const -> std::expected<std::span<const std::uint32_t>, Error> {
		return field(catalog_.meshes, mesh, &AssetMesh::indices).transform([](const auto *values) { return std::span{*values}; });
	}

	/// @brief Derives the vertex count from the shared vertex buffer.
	auto AssetSystem::meshVertexCount(MeshHandle mesh) const -> std::expected<VertexCount, Error> {
		const auto source = require(catalog_.meshes, mesh);
		if (!source) { return std::unexpected(source.error()); }
		return VertexCount{.value = (*source)->vertices->size()};
	}

	/// @brief Derives the index count from the stored triangle index array.
	auto AssetSystem::meshIndexCount(MeshHandle mesh) const -> std::expected<IndexCount, Error> {
		const auto source = require(catalog_.meshes, mesh);
		if (!source) { return std::unexpected(source.error()); }
		return IndexCount{.value = (*source)->indices.size()};
	}

	/// @brief Counts distinct source paths across all materials of a scene, including embedded image keys.
	auto AssetSystem::sceneTextureCount(SceneHandle scene) const -> std::expected<std::size_t, Error> {
		const auto source = require(catalog_.scenes, scene);
		if (!source) { return std::unexpected(source.error()); }
		std::set<std::filesystem::path> paths{};
		// Shared paths count once even when several materials or semantics reference them.
		for (const auto handle : (*source)->materials) {
			const auto material = require(catalog_.materials, handle);
			if (!material) { return std::unexpected(material.error()); }
			for (const auto &texture : (*material)->texture_sources) { paths.insert(texture.path); }
		}
		return paths.size();
	}

	auto AssetSystem::addScene(ObjectName name)												-> std::expected<SceneHandle, Error>{
		auto scene = AssetScene{.handle = makeCounterHandle<SceneHandle>(), .name = std::move(name)};
		const auto handle = scene.handle;
		if (auto added = catalog_.scenes.add(std::move(scene)); !added) { return std::unexpected(added.error()); }
		return handle;
	}

	auto AssetSystem::loadScene(const std::filesystem::path &source)					-> std::expected<SceneHandle, Error>{
		if (source.empty()) { return std::unexpected(Error::invalid_argument); }
		Assimp::Importer importer{};
		constexpr auto flags = aiProcess_Triangulate | aiProcess_JoinIdenticalVertices |
										aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace |
										aiProcess_ImproveCacheLocality;
		const auto path = normalized(source);
		const aiScene *scene = importer.ReadFile(path.string(), flags);
		if (scene == nullptr || scene->mRootNode == nullptr) { return std::unexpected(Error::asset_import_failed); }
		return import(catalog_, *scene, path);
	}

} // namespace vve::simple
