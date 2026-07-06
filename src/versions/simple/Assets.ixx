module;

#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <cmath>
#include <cstdlib>

#if defined(_WIN32) && defined(VVE_ENGINE_BUILD)
#define VVE_SIMPLE_API __declspec(dllexport)
#else
#define VVE_SIMPLE_API
#endif

export module VEEngine.Simple:Assets;
import std;
export import :Types;
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
		Vector<MaterialHandle> materials{};																	///< Materials used by attached meshes.
	};

	/// @brief Mesh descriptor with just enough information for examples and future upload.
	struct AssetMesh {
		using Handle = MeshHandle;																				///< Handle category.
		MeshHandle handle{};																						///< Stable mesh handle.
		ObjectName name{};																						///< Imported mesh name.
		VertexCount vertex_count{};																			///< Source vertex count.
		IndexCount index_count{};																				///< Source index count.
		MaterialHandle material{};																				///< Default material.
		Bounds bounds{};																							///< Object-space bounds.
		Vector<Vec3> positions{};																				///< Imported vertex positions.
		Vector<Vec3> normals{};																					///< Imported vertex normals.
		Vector<Vec2> texcoords{};																				///< Imported first UV set.
		Vector<std::uint32_t> indices{};																		///< Imported triangle indices.
	};

	/// @brief Material descriptor containing only texture handles.
	struct Material {
		using Handle = MaterialHandle;																		///< Handle category.
		MaterialHandle handle{};																				///< Stable material handle.
		ObjectName name{};																						///< Imported material name.
		Vector<TextureHandle> textures{};																	///< Texture handles referenced by this material.
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
		Vector<NodeHandle> nodes{};																			///< All node handles.
		Vector<MeshHandle> meshes{};																			///< All mesh handles.
		Vector<MaterialHandle> materials{};																	///< All material handles.
		Vector<TextureHandle> textures{};																	///< All texture handles.
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
		[[nodiscard]] VVE_SIMPLE_API auto addScene(ObjectName name)																-> std::expected<SceneHandle, Error>;
		[[nodiscard]] VVE_SIMPLE_API auto loadScene(const std::filesystem::path &source)									-> std::expected<SceneHandle, Error>;
		[[nodiscard]] VVE_SIMPLE_API auto containsScene(SceneHandle scene) const											-> bool;
		[[nodiscard]] VVE_SIMPLE_API auto sceneName(SceneHandle scene) const													-> std::expected<ObjectName, Error>;
		[[nodiscard]] VVE_SIMPLE_API auto sceneNodeCount(SceneHandle scene) const											-> std::expected<std::size_t, Error>;
		[[nodiscard]] VVE_SIMPLE_API auto sceneMeshCount(SceneHandle scene) const											-> std::expected<std::size_t, Error>;
		[[nodiscard]] VVE_SIMPLE_API auto sceneMaterialCount(SceneHandle scene) const										-> std::expected<std::size_t, Error>;
		[[nodiscard]] VVE_SIMPLE_API auto sceneTextureCount(SceneHandle scene) const										-> std::expected<std::size_t, Error>;
		[[nodiscard]] VVE_SIMPLE_API auto sceneLightCount(SceneHandle scene) const											-> std::expected<std::size_t, Error>;
		[[nodiscard]] VVE_SIMPLE_API auto sceneCameraCount(SceneHandle scene) const										-> std::expected<std::size_t, Error>;
		[[nodiscard]] VVE_SIMPLE_API auto sceneRootNode(SceneHandle scene) const											-> std::expected<NodeHandle, Error>;
		[[nodiscard]] VVE_SIMPLE_API auto sceneNodes(SceneHandle scene) const												-> std::expected<Vector<NodeHandle>, Error>;
		[[nodiscard]] VVE_SIMPLE_API auto sceneMeshes(SceneHandle scene) const												-> std::expected<Vector<MeshHandle>, Error>;
		[[nodiscard]] VVE_SIMPLE_API auto sceneMaterials(SceneHandle scene) const											-> std::expected<Vector<MaterialHandle>, Error>;
		[[nodiscard]] VVE_SIMPLE_API auto sceneTextures(SceneHandle scene) const											-> std::expected<Vector<TextureHandle>, Error>;
		[[nodiscard]] VVE_SIMPLE_API auto sceneLights(SceneHandle scene) const												-> std::expected<Vector<LightHandle>, Error>;
		[[nodiscard]] VVE_SIMPLE_API auto sceneCameras(SceneHandle scene) const												-> std::expected<Vector<CameraHandle>, Error>;
		[[nodiscard]] VVE_SIMPLE_API auto lightData(LightHandle light) const												-> std::expected<LightDescriptor, Error>;
		[[nodiscard]] VVE_SIMPLE_API auto cameraData(CameraHandle camera) const											-> std::expected<CameraDescriptor, Error>;
		[[nodiscard]] VVE_SIMPLE_API std::expected<Vector<NodeHandle>, Error> sceneNodeChildren(SceneHandle scene,
																											NodeHandle node) const;
		[[nodiscard]] VVE_SIMPLE_API std::expected<std::optional<NodeHandle>, Error> sceneNodeParent(SceneHandle scene,
																												NodeHandle node) const;

		[[nodiscard]] VVE_SIMPLE_API auto nodeName(NodeHandle node) const														-> std::expected<ObjectName, Error>;
		[[nodiscard]] VVE_SIMPLE_API auto nodeTransform(NodeHandle node) const												-> std::expected<Transform, Error>;
		[[nodiscard]] VVE_SIMPLE_API auto nodeMeshes(NodeHandle node) const													-> std::expected<Vector<MeshHandle>, Error>;
		[[nodiscard]] VVE_SIMPLE_API auto nodeMaterials(NodeHandle node) const												-> std::expected<Vector<MaterialHandle>, Error>;
		[[nodiscard]] VVE_SIMPLE_API auto meshName(MeshHandle mesh) const														-> std::expected<ObjectName, Error>;
		[[nodiscard]] VVE_SIMPLE_API auto meshVertexCount(MeshHandle mesh) const											-> std::expected<VertexCount, Error>;
		[[nodiscard]] VVE_SIMPLE_API auto meshIndexCount(MeshHandle mesh) const												-> std::expected<IndexCount, Error>;
		[[nodiscard]] VVE_SIMPLE_API auto meshMaterial(MeshHandle mesh) const												-> std::expected<MaterialHandle, Error>;
		[[nodiscard]] VVE_SIMPLE_API auto meshBounds(MeshHandle mesh) const													-> std::expected<Bounds, Error>;
		[[nodiscard]] VVE_SIMPLE_API auto meshPositions(MeshHandle mesh) const												-> std::expected<Vector<Vec3>, Error>;
		[[nodiscard]] VVE_SIMPLE_API auto meshNormals(MeshHandle mesh) const													-> std::expected<Vector<Vec3>, Error>;
		[[nodiscard]] VVE_SIMPLE_API auto meshTexcoords(MeshHandle mesh) const												-> std::expected<Vector<Vec2>, Error>;
		[[nodiscard]] VVE_SIMPLE_API auto meshIndices(MeshHandle mesh) const													-> std::expected<Vector<std::uint32_t>, Error>;
		[[nodiscard]] VVE_SIMPLE_API auto materialName(MaterialHandle material) const										-> std::expected<ObjectName, Error>;
		[[nodiscard]] VVE_SIMPLE_API auto materialTextures(MaterialHandle material) const								-> std::expected<Vector<TextureHandle>, Error>;

	private:
		/// @brief Result of importing all materials.
		struct MaterialImport {
			Vector<MaterialHandle> materials{};	///< Material handles by Assimp material index.
			Vector<TextureHandle> textures{};	///< Unique texture handles referenced by the scene.
		};

		/// @brief Common texture slots students expect when inspecting imported materials.
		inline static constexpr std::array texture_types{aiTextureType_DIFFUSE, aiTextureType_BASE_COLOR,
																			aiTextureType_NORMALS, aiTextureType_HEIGHT,
																			aiTextureType_DIFFUSE_ROUGHNESS, aiTextureType_METALNESS,
																			aiTextureType_EMISSIVE, aiTextureType_AMBIENT_OCCLUSION,
																			aiTextureType_LIGHTMAP};

		[[nodiscard]] static auto normalized(std::filesystem::path path)									-> std::filesystem::path;
		[[nodiscard]] static auto vec3(const aiVector3D &v)													-> Vec3;
		[[nodiscard]] static auto quat(const aiQuaternion &q)													-> Quat;
		[[nodiscard]] static auto transform(const aiMatrix4x4 &matrix)										-> Transform;
		[[nodiscard]] static auto color(const aiColor3D &value)												-> LinearColor;
		[[nodiscard]] static auto lightRange(const aiLight &source)											-> LightRange;
		[[nodiscard]] static auto lightDescriptor(const aiLight &source)									-> std::optional<LightDescriptor>;
		[[nodiscard]] static auto cameraAspect(const aiCamera &source)										-> Scalar;
		[[nodiscard]] static auto cameraFovY(const aiCamera &source)										-> FovY;
		[[nodiscard]] static auto cameraDescriptor(const aiCamera &source)								-> CameraDescriptor;
		[[nodiscard]] static auto name(const aiString &text, std::string fallback)						-> std::string;
		[[nodiscard]] static std::filesystem::path texturePath(const aiString &path,
																					const std::filesystem::path &scene_dir);
		template <typename T>
		[[nodiscard]] static std::expected<const T *, Error> require(const Table<T> &table,
																							typename T::Handle handle);
		template <typename T> [[nodiscard]] static bool contains(const Vector<T> &values, T value);
		template <typename T>
		[[nodiscard]] static std::expected<T, Error> sceneField(const Catalog &catalog, SceneHandle handle,
																					T AssetScene::*field);
		template <typename Descriptor, typename T>
		[[nodiscard]] static std::expected<T, Error> field(const Table<Descriptor> &table,
																			typename Descriptor::Handle handle,
																			T Descriptor::*member);
		template <typename T>
		[[nodiscard]] static std::expected<std::size_t, Error>
		sizeOf(std::expected<Vector<T>, Error> value);
		[[nodiscard]] static std::expected<const AssetScene *, Error> sceneWithNode(const Catalog &catalog,
																										SceneHandle scene,
																										NodeHandle node);
		[[nodiscard]] static TextureHandle texture(const std::filesystem::path &source,
																	std::map<std::string, TextureHandle> &known,
																	Vector<TextureHandle> &scene_textures);
		[[nodiscard]] static std::expected<MaterialImport, Error> materials(Catalog &catalog, const aiScene &scene,
																									const std::filesystem::path &scene_dir);
		[[nodiscard]] static auto boundsOf(const aiMesh &source)												-> Bounds;
		[[nodiscard]] static auto indexCount(const aiMesh &source)											-> std::uint64_t;
		[[nodiscard]] static std::expected<Vector<MeshHandle>, Error>
		meshes(Catalog &catalog, const aiScene &scene, const Vector<MaterialHandle> &material_handles);
		[[nodiscard]] static std::expected<NodeHandle, Error> node(Catalog &catalog, const aiScene &source_scene,
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

	template <typename T> using Expected = std::expected<T, Error>;	///< Local expected shorthand.
	template <typename T> using VectorExpected = Expected<Vector<T>>;	///< Local vector-result shorthand.
	using CountExpected = Expected<std::size_t>;								///< Local count-result shorthand.
	using NameExpected = Expected<ObjectName>;								///< Local name-result shorthand.

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

		LightRange AssetSystem::lightRange(const aiLight &source) {								///< Derives a finite range from attenuation when present.
			if (source.mAttenuationLinear > 0.0F) {
				return LightRange{.value = static_cast<Scalar>(1.0F / source.mAttenuationLinear)};
			}
			if (source.mAttenuationQuadratic > 0.0F) {
				return LightRange{.value = static_cast<Scalar>(1.0F / std::sqrt(source.mAttenuationQuadratic))};
			}
			return {};
		}

		std::optional<LightDescriptor> AssetSystem::lightDescriptor(const aiLight &source) {	///< Converts supported Assimp lights.
			LightDescriptor data{.color = color(source.mColorDiffuse),
										.intensity = LightIntensity{.value = one()},
										.direction = Direction{.value = vec3(source.mDirection)},
										.position = Position{.value = vec3(source.mPosition)},
										.range = lightRange(source),
										.cone = SpotConeAngle{.radians = static_cast<Scalar>(source.mAngleOuterCone)}};
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

		CameraDescriptor AssetSystem::cameraDescriptor(const aiCamera &source) {				///< Converts Assimp camera data.
			return CameraDescriptor{.position = Position{.value = vec3(source.mPosition)},
											.direction = Direction{.value = vec3(source.mLookAt)},
											.up = Direction{.value = vec3(source.mUp)},
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
			if (result.empty() || result.is_absolute() || result.string().starts_with('*')) { return result; }
			return normalized(scene_dir / result);
		}

		template <typename T>
		auto AssetSystem::require(const Table<T> &table, typename T::Handle handle)					-> std::expected<const T *, Error>{
			const auto *value = table.find(handle);
			if (value == nullptr) { return std::unexpected(Error::missing_object); }
			return value;
		}

		template <typename T> bool AssetSystem::contains(const Vector<T> &values, T value) {
			return std::ranges::find(values, value) != values.end();
		}

		template <typename T>
		auto AssetSystem::sceneField(const Catalog &catalog, SceneHandle handle, T AssetScene::*field)	-> std::expected<T, Error>{
			const auto scene = require(catalog.scenes, handle);
			if (!scene) { return std::unexpected(scene.error()); }
			return (*scene)->*field;
		}

		template <typename Descriptor, typename T>
		std::expected<T, Error> AssetSystem::field(const Table<Descriptor> &table,
																	typename Descriptor::Handle handle, T Descriptor::*member) {
			const auto descriptor = require(table, handle);
			if (!descriptor) { return std::unexpected(descriptor.error()); }
			return (*descriptor)->*member;
		}

		template <typename T>
		auto AssetSystem::sizeOf(std::expected<Vector<T>, Error> value)									-> std::expected<std::size_t, Error>{
			if (!value) { return std::unexpected(value.error()); }
			return value->size();
		}

		std::expected<const AssetScene *, Error> AssetSystem::sceneWithNode(const Catalog &catalog, SceneHandle scene,
																							NodeHandle node) {
			const auto value = require(catalog.scenes, scene);
			if (!value) { return std::unexpected(value.error()); }
			if (!contains((*value)->nodes, node)) { return std::unexpected(Error::missing_object); }
			return *value;
		}

		TextureHandle AssetSystem::texture(const std::filesystem::path &source,
														std::map<std::string, TextureHandle> &known,
														Vector<TextureHandle> &scene_textures) {
			const auto key = source.string();
			if (const auto it = known.find(key); it != known.end()) { return it->second; }
			const auto handle = makeCounterHandle<TextureHandle>();
			known.emplace(key, handle);
			scene_textures.push_back(handle);
			return handle;
		}

		std::expected<AssetSystem::MaterialImport, Error>
		AssetSystem::materials(Catalog &catalog, const aiScene &scene, const std::filesystem::path &scene_dir) {
			MaterialImport result{.materials = Vector<MaterialHandle>(scene.mNumMaterials)};
			std::map<std::string, TextureHandle> known_textures{};
			for (unsigned i = 0; i < scene.mNumMaterials; ++i) {
				const auto *source = scene.mMaterials[i];
				aiString material_name{};
				if (source != nullptr) { source->Get(AI_MATKEY_NAME, material_name); }

				auto item = Material{.handle = makeCounterHandle<MaterialHandle>(),
											.name = ObjectName{.value = name(material_name, "Material_" + std::to_string(i))}};
				if (source != nullptr) {
					for (const auto type : texture_types) {
						for (unsigned slot = 0; slot < source->GetTextureCount(type); ++slot) {
							aiString path{};
							if (source->GetTexture(type, slot, &path) != AI_SUCCESS) { continue; }
							item.textures.push_back(texture(texturePath(path, scene_dir), known_textures, result.textures));
						}
					}
				}
				if (auto added = catalog.materials.add(item); !added) { return std::unexpected(added.error()); }
				result.materials[i] = item.handle;
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
				item.positions.reserve(source->mNumVertices);
				item.normals.reserve(source->mNumVertices);
				item.texcoords.reserve(source->mNumVertices);
				for (unsigned vertex = 0; vertex < source->mNumVertices; ++vertex) {
					item.positions.push_back(source->mVertices != nullptr ? vec3(source->mVertices[vertex]) : zeroVec3());
					item.normals.push_back(source->HasNormals() ? vec3(source->mNormals[vertex]) : zeroVec3());
					const auto uv = source->HasTextureCoords(0) ? source->mTextureCoords[0][vertex] : aiVector3D{};
					item.texcoords.push_back(Vec2{uv.x, uv.y});
				}
				item.indices.reserve(static_cast<std::size_t>(indexCount(*source)));
				for (unsigned face = 0; face < source->mNumFaces; ++face) {
					const auto &source_face = source->mFaces[face];
					for (unsigned index = 0; index < source_face.mNumIndices; ++index) {
						item.indices.push_back(source_face.mIndices[index]);
					}
				}
				item.vertex_count = VertexCount{.value = item.positions.size()};
				item.index_count = IndexCount{.value = item.indices.size()};
				if (auto added = catalog.meshes.add(item); !added) { return std::unexpected(added.error()); }
				result[i] = item.handle;
			}
			return result;
		}

		std::expected<NodeHandle, Error>
		AssetSystem::node(Catalog &catalog, const aiScene &source_scene, const aiNode &source,
								const Vector<MeshHandle> &mesh_handles, AssetScene &scene, NodeHandle parent) {
			auto item = Node{.handle = makeCounterHandle<NodeHandle>(),
									.name = ObjectName{.value = name(source.mName, "Node_" + std::to_string(scene.nodes.size()))},
									.transform = transform(source.mTransformation)};
			for (unsigned slot = 0; slot < source.mNumMeshes; ++slot) {
				const auto mesh_index = source.mMeshes[slot];
				if (mesh_index >= mesh_handles.size() || !mesh_handles[mesh_index].valid()) { continue; }
				const auto *mesh = source_scene.mMeshes[mesh_index];
				item.meshes.push_back(mesh_handles[mesh_index]);
				item.materials.push_back(mesh != nullptr && mesh->mMaterialIndex < scene.materials.size()
														? scene.materials[mesh->mMaterialIndex]
														: MaterialHandle{});
			}

			const auto handle = item.handle;
			if (auto added = catalog.nodes.add(std::move(item)); !added) { return std::unexpected(added.error()); }
			const auto linked = parent.valid() ? scene.tree.addChild(parent, handle) : scene.tree.setRoot(handle);
			if (!linked) { return std::unexpected(linked.error()); }
			scene.nodes.push_back(handle);

			for (unsigned i = 0; i < source.mNumChildren; ++i) {
				if (source.mChildren[i] == nullptr) { continue; }
				if (auto child = node(catalog, source_scene, *source.mChildren[i], mesh_handles, scene, handle); !child) {
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
				auto data = lightDescriptor(*source);
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
				auto item = CameraAsset{.handle = makeCounterHandle<CameraHandle>(), .data = cameraDescriptor(*source)};
				const auto handle = item.handle;
				if (auto added = catalog.cameras.add(std::move(item)); !added) { return std::unexpected(added.error()); }
				result.push_back(handle);
			}
			return result;
		}

		std::expected<SceneHandle, Error>
		AssetSystem::import(Catalog &catalog, const aiScene &source, const std::filesystem::path &path) {
			const auto imported_materials = materials(catalog, source, path.parent_path());
			if (!imported_materials) { return std::unexpected(imported_materials.error()); }
			const auto imported_meshes = meshes(catalog, source, imported_materials->materials);
			if (!imported_meshes) { return std::unexpected(imported_meshes.error()); }
			const auto imported_lights = lights(catalog, source);
			if (!imported_lights) { return std::unexpected(imported_lights.error()); }
			const auto imported_cameras = cameras(catalog, source);
			if (!imported_cameras) { return std::unexpected(imported_cameras.error()); }

			auto scene = AssetScene{.handle = makeCounterHandle<SceneHandle>(),
										.name = ObjectName{.value = path.filename().string()},
										.meshes = *imported_meshes,
										.materials = imported_materials->materials,
										.textures = imported_materials->textures,
										.lights = *imported_lights,
										.cameras = *imported_cameras};
			if (source.mRootNode != nullptr) {
				if (auto root = node(catalog, source, *source.mRootNode, *imported_meshes, scene); !root) {
					return std::unexpected(root.error());
				}
			}
			const auto handle = scene.handle;
			if (auto added = catalog.scenes.add(std::move(scene)); !added) { return std::unexpected(added.error()); }
			return handle;
		}

	VVE_SIMPLE_API auto AssetSystem::addScene(ObjectName name)												-> std::expected<SceneHandle, Error>{
		auto scene = AssetScene{.handle = makeCounterHandle<SceneHandle>(), .name = std::move(name)};
		const auto handle = scene.handle;
		if (auto added = catalog_.scenes.add(std::move(scene)); !added) { return std::unexpected(added.error()); }
		return handle;
	}

	VVE_SIMPLE_API auto AssetSystem::loadScene(const std::filesystem::path &source)					-> std::expected<SceneHandle, Error>{
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

	VVE_SIMPLE_API bool AssetSystem::containsScene(SceneHandle scene) const { return catalog_.scenes.contains(scene); }

	VVE_SIMPLE_API NameExpected AssetSystem::sceneName(SceneHandle scene) const { return sceneField(catalog_, scene, &AssetScene::name); }

	VVE_SIMPLE_API CountExpected AssetSystem::sceneNodeCount(SceneHandle scene) const { return sizeOf(sceneNodes(scene)); }

	VVE_SIMPLE_API CountExpected AssetSystem::sceneMeshCount(SceneHandle scene) const { return sizeOf(sceneMeshes(scene)); }

	VVE_SIMPLE_API auto AssetSystem::sceneMaterialCount(SceneHandle scene) const						-> CountExpected{
		return sizeOf(sceneMaterials(scene));
	}

	VVE_SIMPLE_API auto AssetSystem::sceneTextureCount(SceneHandle scene) const						-> CountExpected{
		return sizeOf(sceneTextures(scene));
	}

	VVE_SIMPLE_API CountExpected AssetSystem::sceneLightCount(SceneHandle scene) const { return sizeOf(sceneLights(scene)); }

	VVE_SIMPLE_API CountExpected AssetSystem::sceneCameraCount(SceneHandle scene) const { return sizeOf(sceneCameras(scene)); }

	VVE_SIMPLE_API auto AssetSystem::sceneRootNode(SceneHandle scene) const								-> std::expected<NodeHandle, Error>{
		const auto root = sceneField(catalog_, scene, &AssetScene::tree);
		if (!root) { return std::unexpected(root.error()); }
		if (!root->root.valid()) { return std::unexpected(Error::missing_object); }
		return root->root;
	}

	VVE_SIMPLE_API auto AssetSystem::sceneNodes(SceneHandle scene) const									-> VectorExpected<NodeHandle>{
		return sceneField(catalog_, scene, &AssetScene::nodes);
	}

	VVE_SIMPLE_API auto AssetSystem::sceneMeshes(SceneHandle scene) const								-> VectorExpected<MeshHandle>{
		return sceneField(catalog_, scene, &AssetScene::meshes);
	}

	VVE_SIMPLE_API auto AssetSystem::sceneMaterials(SceneHandle scene) const							-> VectorExpected<MaterialHandle>{
		return sceneField(catalog_, scene, &AssetScene::materials);
	}

	VVE_SIMPLE_API auto AssetSystem::sceneTextures(SceneHandle scene) const								-> VectorExpected<TextureHandle>{
		return sceneField(catalog_, scene, &AssetScene::textures);
	}

	VVE_SIMPLE_API auto AssetSystem::sceneLights(SceneHandle scene) const								-> VectorExpected<LightHandle>{
		return sceneField(catalog_, scene, &AssetScene::lights);
	}

	VVE_SIMPLE_API auto AssetSystem::sceneCameras(SceneHandle scene) const								-> VectorExpected<CameraHandle>{
		return sceneField(catalog_, scene, &AssetScene::cameras);
	}

	VVE_SIMPLE_API auto AssetSystem::lightData(LightHandle light) const									-> Expected<LightDescriptor>{
		return field(catalog_.lights, light, &Light::data);
	}

	VVE_SIMPLE_API auto AssetSystem::cameraData(CameraHandle camera) const								-> Expected<CameraDescriptor>{
		return field(catalog_.cameras, camera, &CameraAsset::data);
	}

	VVE_SIMPLE_API auto AssetSystem::sceneNodeChildren(SceneHandle scene, NodeHandle node) const	-> std::expected<Vector<NodeHandle>, Error>{
		const auto source = sceneWithNode(catalog_, scene, node);
		if (!source) { return std::unexpected(source.error()); }
		return (*source)->tree.children(node);
	}

	VVE_SIMPLE_API std::expected<std::optional<NodeHandle>, Error> AssetSystem::sceneNodeParent(SceneHandle scene,
																											NodeHandle node) const {
		const auto source = sceneWithNode(catalog_, scene, node);
		if (!source) { return std::unexpected(source.error()); }
		return (*source)->tree.parent(node);
	}

	VVE_SIMPLE_API NameExpected AssetSystem::nodeName(NodeHandle node) const { return field(catalog_.nodes, node, &Node::name); }

	VVE_SIMPLE_API auto AssetSystem::nodeTransform(NodeHandle node) const								-> Expected<Transform>{
		return field(catalog_.nodes, node, &Node::transform);
	}

	VVE_SIMPLE_API auto AssetSystem::nodeMeshes(NodeHandle node) const									-> VectorExpected<MeshHandle>{
		return field(catalog_.nodes, node, &Node::meshes);
	}

	VVE_SIMPLE_API auto AssetSystem::nodeMaterials(NodeHandle node) const								-> VectorExpected<MaterialHandle>{
		return field(catalog_.nodes, node, &Node::materials);
	}

	VVE_SIMPLE_API NameExpected AssetSystem::meshName(MeshHandle mesh) const { return field(catalog_.meshes, mesh, &AssetMesh::name); }

	VVE_SIMPLE_API auto AssetSystem::meshVertexCount(MeshHandle mesh) const								-> Expected<VertexCount>{
		return field(catalog_.meshes, mesh, &AssetMesh::vertex_count);
	}

	VVE_SIMPLE_API auto AssetSystem::meshIndexCount(MeshHandle mesh) const								-> Expected<IndexCount>{
		return field(catalog_.meshes, mesh, &AssetMesh::index_count);
	}

	VVE_SIMPLE_API auto AssetSystem::meshMaterial(MeshHandle mesh) const									-> Expected<MaterialHandle>{
		return field(catalog_.meshes, mesh, &AssetMesh::material);
	}

	VVE_SIMPLE_API auto AssetSystem::meshBounds(MeshHandle mesh) const									-> Expected<Bounds>{
		return field(catalog_.meshes, mesh, &AssetMesh::bounds);
	}

	VVE_SIMPLE_API auto AssetSystem::meshPositions(MeshHandle mesh) const								-> VectorExpected<Vec3>{
		return field(catalog_.meshes, mesh, &AssetMesh::positions);
	}

	VVE_SIMPLE_API auto AssetSystem::meshNormals(MeshHandle mesh) const									-> VectorExpected<Vec3>{
		return field(catalog_.meshes, mesh, &AssetMesh::normals);
	}

	VVE_SIMPLE_API auto AssetSystem::meshTexcoords(MeshHandle mesh) const								-> VectorExpected<Vec2>{
		return field(catalog_.meshes, mesh, &AssetMesh::texcoords);
	}

	VVE_SIMPLE_API auto AssetSystem::meshIndices(MeshHandle mesh) const									-> VectorExpected<std::uint32_t>{
		return field(catalog_.meshes, mesh, &AssetMesh::indices);
	}

	VVE_SIMPLE_API auto AssetSystem::materialName(MaterialHandle material) const						-> NameExpected{
		return field(catalog_.materials, material, &Material::name);
	}

	VVE_SIMPLE_API auto AssetSystem::materialTextures(MaterialHandle material) const					-> VectorExpected<TextureHandle>{
		return field(catalog_.materials, material, &Material::textures);
	}

} // namespace vve::simple
