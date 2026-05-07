module;

#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <cstdlib>

export module VEEngine.V4:Assets;
import std;
export import :Types;

/// @file
/// @brief Compact Assimp-backed asset system for the educational v4 engine.

namespace vve::v4 {

   /// @brief Minimal handle table used by all imported descriptor types.
   template <typename T> struct Table {
      using Handle = typename T::Handle;                      ///< Strong handle type accepted by this table.
      std::map<Handle, T> data{};                             ///< Ordered descriptor storage.

      [[nodiscard]] std::expected<void, Error> add(T value) {  ///< Stores one descriptor by its own handle.
         if (!value.handle.valid()) { return std::unexpected(Error::invalid_handle); }
         if (auto [_, ok] = data.emplace(value.handle, std::move(value)); !ok) {
            return std::unexpected(Error::duplicate_object);
         }
         return {};
      }

      [[nodiscard]] const T *find(Handle handle) const {       ///< Finds a descriptor or returns nullptr.
         const auto it = data.find(handle);
         return it == data.end() ? nullptr : std::addressof(it->second);
      }

      [[nodiscard]] bool contains(Handle handle) const { return data.contains(handle); } ///< Tests membership.
   };

   /// @brief Imported scene tree: root handle plus parent/child edge maps.
   struct Tree {
      NodeHandle root{}; ///< Root node handle.
      std::unordered_multimap<NodeHandle, NodeHandle, HandleHash<NodeHandle>> children{}; ///< Parent to children.
      std::unordered_map<NodeHandle, NodeHandle, HandleHash<NodeHandle>> parents{};       ///< Child to parent.
   };

   /// @brief Scene node descriptor.
   struct Node {
      using Handle = NodeHandle;       ///< Handle category.
      NodeHandle handle{};             ///< Stable node handle.
      ObjectName name{};               ///< Imported node name.
      Transform transform{};           ///< Local transform.
      Vector<MeshHandle> meshes{};     ///< Meshes attached to this node.
      Vector<MaterialHandle> materials{}; ///< Materials used by attached meshes.
   };

   /// @brief Mesh descriptor with just enough information for examples and future upload.
   struct Mesh {
      using Handle = MeshHandle;       ///< Handle category.
      MeshHandle handle{};             ///< Stable mesh handle.
      ObjectName name{};               ///< Imported mesh name.
      VertexCount vertex_count{};      ///< Source vertex count.
      IndexCount index_count{};        ///< Source index count.
      MaterialHandle material{};       ///< Default material.
      Bounds bounds{};                 ///< Object-space bounds.
   };

   /// @brief Material descriptor containing only texture handles.
   struct Material {
      using Handle = MaterialHandle;   ///< Handle category.
      MaterialHandle handle{};         ///< Stable material handle.
      ObjectName name{};               ///< Imported material name.
      Vector<TextureHandle> textures{}; ///< Texture handles referenced by this material.
   };

   /// @brief Scene descriptor stores handle lists; details live in the catalog tables.
   struct Scene {
      using Handle = SceneHandle;      ///< Handle category.
      SceneHandle handle{};            ///< Stable scene handle.
      ObjectName name{};               ///< Source file name.
      Tree tree{};                     ///< Parent/child node topology.
      Vector<NodeHandle> nodes{};      ///< All node handles.
      Vector<MeshHandle> meshes{};     ///< All mesh handles.
      Vector<MaterialHandle> materials{}; ///< All material handles.
      Vector<TextureHandle> textures{}; ///< All texture handles.
      Vector<LightHandle> lights{};    ///< All light handles.
      Vector<CameraHandle> cameras{};  ///< All camera handles.
   };

   /// @brief All imported descriptors, keyed by stable typed handles.
   struct Catalog {
      Table<Scene> scenes{};           ///< Scenes by handle.
      Table<Node> nodes{};             ///< Nodes by handle.
      Table<Mesh> meshes{};            ///< Meshes by handle.
      Table<Material> materials{};     ///< Materials by handle.
   };

} // namespace vve::v4

export namespace vve::v4 {

   /// @brief Asset facade that owns imported scene descriptors.
   class AssetSystem {
   public:
      [[nodiscard]] std::expected<SceneHandle, Error> addScene(ObjectName name);
      [[nodiscard]] std::expected<SceneHandle, Error> loadScene(const std::filesystem::path &source);
      [[nodiscard]] bool containsScene(SceneHandle scene) const;
      [[nodiscard]] std::expected<ObjectName, Error> sceneName(SceneHandle scene) const;
      [[nodiscard]] std::expected<std::size_t, Error> sceneNodeCount(SceneHandle scene) const;
      [[nodiscard]] std::expected<std::size_t, Error> sceneMeshCount(SceneHandle scene) const;
      [[nodiscard]] std::expected<std::size_t, Error> sceneMaterialCount(SceneHandle scene) const;
      [[nodiscard]] std::expected<std::size_t, Error> sceneTextureCount(SceneHandle scene) const;
      [[nodiscard]] std::expected<std::size_t, Error> sceneLightCount(SceneHandle scene) const;
      [[nodiscard]] std::expected<std::size_t, Error> sceneCameraCount(SceneHandle scene) const;
      [[nodiscard]] std::expected<NodeHandle, Error> sceneRootNode(SceneHandle scene) const;
      [[nodiscard]] std::expected<Vector<NodeHandle>, Error> sceneNodes(SceneHandle scene) const;
      [[nodiscard]] std::expected<Vector<MeshHandle>, Error> sceneMeshes(SceneHandle scene) const;
      [[nodiscard]] std::expected<Vector<MaterialHandle>, Error> sceneMaterials(SceneHandle scene) const;
      [[nodiscard]] std::expected<Vector<TextureHandle>, Error> sceneTextures(SceneHandle scene) const;
      [[nodiscard]] std::expected<Vector<LightHandle>, Error> sceneLights(SceneHandle scene) const;
      [[nodiscard]] std::expected<Vector<CameraHandle>, Error> sceneCameras(SceneHandle scene) const;
      [[nodiscard]] std::expected<Vector<NodeHandle>, Error> sceneNodeChildren(SceneHandle scene,
                                                                               NodeHandle node) const;
      [[nodiscard]] std::expected<std::optional<NodeHandle>, Error> sceneNodeParent(SceneHandle scene,
                                                                                    NodeHandle node) const;

      [[nodiscard]] std::expected<ObjectName, Error> nodeName(NodeHandle node) const;
      [[nodiscard]] std::expected<Transform, Error> nodeTransform(NodeHandle node) const;
      [[nodiscard]] std::expected<Vector<MeshHandle>, Error> nodeMeshes(NodeHandle node) const;
      [[nodiscard]] std::expected<Vector<MaterialHandle>, Error> nodeMaterials(NodeHandle node) const;
      [[nodiscard]] std::expected<ObjectName, Error> meshName(MeshHandle mesh) const;
      [[nodiscard]] std::expected<VertexCount, Error> meshVertexCount(MeshHandle mesh) const;
      [[nodiscard]] std::expected<IndexCount, Error> meshIndexCount(MeshHandle mesh) const;
      [[nodiscard]] std::expected<MaterialHandle, Error> meshMaterial(MeshHandle mesh) const;
      [[nodiscard]] std::expected<Bounds, Error> meshBounds(MeshHandle mesh) const;
      [[nodiscard]] std::expected<ObjectName, Error> materialName(MaterialHandle material) const;
      [[nodiscard]] std::expected<Vector<TextureHandle>, Error> materialTextures(MaterialHandle material) const;

   private:
      Catalog catalog_{}; ///< All descriptors loaded through this asset system.
   };

} // namespace vve::v4

namespace vve::v4 {

   namespace {

      template <typename T> using Expected = std::expected<T, Error>;           ///< Local expected shorthand.
      template <typename T> using VectorExpected = Expected<Vector<T>>;         ///< Local vector-result shorthand.
      using CountExpected = Expected<std::size_t>;                              ///< Local count-result shorthand.
      using NameExpected = Expected<ObjectName>;                                ///< Local name-result shorthand.

      /// @brief Common texture slots students expect when inspecting imported materials.
      constexpr std::array texture_types{aiTextureType_DIFFUSE, aiTextureType_BASE_COLOR, aiTextureType_NORMALS,
                                         aiTextureType_HEIGHT, aiTextureType_DIFFUSE_ROUGHNESS,
                                         aiTextureType_METALNESS, aiTextureType_EMISSIVE,
                                         aiTextureType_AMBIENT_OCCLUSION, aiTextureType_LIGHTMAP};

      /// @brief Result of importing all materials.
      struct MaterialImport {
         Vector<MaterialHandle> materials{}; ///< Material handles by Assimp material index.
         Vector<TextureHandle> textures{};   ///< Unique texture handles referenced by the scene.
      };

      [[nodiscard]] std::filesystem::path normalized(std::filesystem::path path) { ///< Canonical path if possible.
         std::error_code error{};
         const auto canonical = std::filesystem::weakly_canonical(path, error);
         return error ? path.lexically_normal() : canonical;
      }

      [[nodiscard]] Vec3 vec3(const aiVector3D &v) { return Vec3(v.x, v.y, v.z); } ///< Assimp vector conversion.
      [[nodiscard]] Quat quat(const aiQuaternion &q) { return Quat(q.w, q.x, q.y, q.z); } ///< Quaternion conversion.

      [[nodiscard]] Transform transform(const aiMatrix4x4 &matrix) { ///< Converts Assimp local transforms.
         aiVector3D scale{};
         aiQuaternion rotation{};
         aiVector3D translation{};
         matrix.Decompose(scale, rotation, translation);
         return Transform{.translation = Position{.value = vec3(translation)},
                          .rotation = Rotation{.value = quat(rotation)},
                          .scale = Scale{.value = vec3(scale)}};
      }

      [[nodiscard]] std::string name(const aiString &text, std::string fallback) { ///< Name or generated fallback.
         return text.length > 0 ? std::string{text.C_Str()} : std::move(fallback);
      }

      [[nodiscard]] std::filesystem::path texturePath(const aiString &path, const std::filesystem::path &scene_dir) {
         auto result = std::filesystem::path(path.C_Str());
         if (result.empty() || result.is_absolute() || result.string().starts_with('*')) { return result; }
         return normalized(scene_dir / result);
      }

      template <typename T> [[nodiscard]] std::expected<const T *, Error> require(const Table<T> &table,
                                                                                  typename T::Handle handle) {
         const auto *value = table.find(handle);
         if (value == nullptr) { return std::unexpected(Error::missing_object); }
         return value;
      }

      template <typename T> [[nodiscard]] bool contains(const Vector<T> &values, T value) {
         return std::ranges::find(values, value) != values.end();
      }

      template <typename T> [[nodiscard]] std::expected<T, Error> sceneField(const Catalog &catalog,
                                                                             SceneHandle handle,
                                                                             T Scene::*field) {
         const auto scene = require(catalog.scenes, handle);
         if (!scene) { return std::unexpected(scene.error()); }
         return (*scene)->*field;
      }

      template <typename Descriptor, typename T>
      [[nodiscard]] std::expected<T, Error> field(const Table<Descriptor> &table,
                                                  typename Descriptor::Handle handle,
                                                  T Descriptor::*member) {
         const auto descriptor = require(table, handle);
         if (!descriptor) { return std::unexpected(descriptor.error()); }
         return (*descriptor)->*member;
      }

      template <typename T>
      [[nodiscard]] std::expected<std::size_t, Error> sizeOf(std::expected<Vector<T>, Error> value) {
         if (!value) { return std::unexpected(value.error()); }
         return value->size();
      }

      [[nodiscard]] std::expected<const Scene *, Error> sceneWithNode(const Catalog &catalog, SceneHandle scene,
                                                                      NodeHandle node) {
         const auto value = require(catalog.scenes, scene);
         if (!value) { return std::unexpected(value.error()); }
         if (!contains((*value)->nodes, node)) { return std::unexpected(Error::missing_object); }
         return *value;
      }

      [[nodiscard]] TextureHandle texture(const std::filesystem::path &source,
                                          std::map<std::string, TextureHandle> &known,
                                          Vector<TextureHandle> &scene_textures) {
         const auto key = source.string();
         if (const auto it = known.find(key); it != known.end()) { return it->second; }
         const auto handle = makeCounterHandle<TextureHandle>();
         known.emplace(key, handle);
         scene_textures.push_back(handle);
         return handle;
      }

      [[nodiscard]] std::expected<MaterialImport, Error> materials(Catalog &catalog, const aiScene &scene,
                                                                   const std::filesystem::path &scene_dir) {
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

      [[nodiscard]] Bounds boundsOf(const aiMesh &source) { ///< Computes object-space bounds from source vertices.
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

      [[nodiscard]] std::uint64_t indexCount(const aiMesh &source) { ///< Counts all indices in all faces.
         std::uint64_t result = 0;
         for (unsigned face = 0; face < source.mNumFaces; ++face) { result += source.mFaces[face].mNumIndices; }
         return result;
      }

      [[nodiscard]] std::expected<Vector<MeshHandle>, Error> meshes(Catalog &catalog, const aiScene &scene,
                                                                    const Vector<MaterialHandle> &material_handles) {
         Vector<MeshHandle> result(scene.mNumMeshes);
         for (unsigned i = 0; i < scene.mNumMeshes; ++i) {
            const auto *source = scene.mMeshes[i];
            if (source == nullptr) { continue; }
            const auto material = source->mMaterialIndex < material_handles.size()
                                     ? material_handles[source->mMaterialIndex]
                                     : MaterialHandle{};
            auto item = Mesh{.handle = makeCounterHandle<MeshHandle>(),
                             .name = ObjectName{.value = name(source->mName, "Mesh_" + std::to_string(i))},
                             .vertex_count = VertexCount{.value = source->mNumVertices},
                             .index_count = IndexCount{.value = indexCount(*source)},
                             .material = material,
                             .bounds = boundsOf(*source)};
            if (auto added = catalog.meshes.add(item); !added) { return std::unexpected(added.error()); }
            result[i] = item.handle;
         }
         return result;
      }

      [[nodiscard]] std::expected<NodeHandle, Error> node(Catalog &catalog, const aiScene &source_scene,
                                                          const aiNode &source, const Vector<MeshHandle> &mesh_handles,
                                                          Scene &scene, NodeHandle parent = {}) {
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
         if (parent.valid()) {
            scene.tree.children.emplace(parent, handle);
            scene.tree.parents.emplace(handle, parent);
         } else {
            scene.tree.root = handle;
         }
         scene.nodes.push_back(handle);

         for (unsigned i = 0; i < source.mNumChildren; ++i) {
            if (source.mChildren[i] == nullptr) { continue; }
            if (auto child = node(catalog, source_scene, *source.mChildren[i], mesh_handles, scene, handle); !child) {
               return std::unexpected(child.error());
            }
         }
         return handle;
      }

      [[nodiscard]] Vector<LightHandle> lights(const aiScene &scene) {
         Vector<LightHandle> result{};
         result.reserve(scene.mNumLights);
         for (unsigned i = 0; i < scene.mNumLights; ++i) {
            if (scene.mLights[i] != nullptr) { result.push_back(makeCounterHandle<LightHandle>()); }
         }
         return result;
      }

      [[nodiscard]] Vector<CameraHandle> cameras(const aiScene &scene) {
         Vector<CameraHandle> result{};
         result.reserve(scene.mNumCameras);
         for (unsigned i = 0; i < scene.mNumCameras; ++i) {
            if (scene.mCameras[i] != nullptr) { result.push_back(makeCounterHandle<CameraHandle>()); }
         }
         return result;
      }

      [[nodiscard]] std::expected<SceneHandle, Error> import(Catalog &catalog, const aiScene &source,
                                                             const std::filesystem::path &path) {
         const auto imported_materials = materials(catalog, source, path.parent_path());
         if (!imported_materials) { return std::unexpected(imported_materials.error()); }
         const auto imported_meshes = meshes(catalog, source, imported_materials->materials);
         if (!imported_meshes) { return std::unexpected(imported_meshes.error()); }
         const auto imported_lights = lights(source);
         const auto imported_cameras = cameras(source);

         auto scene = Scene{.handle = makeCounterHandle<SceneHandle>(),
                            .name = ObjectName{.value = path.filename().string()},
                            .meshes = *imported_meshes,
                            .materials = imported_materials->materials,
                            .textures = imported_materials->textures,
                            .lights = imported_lights,
                            .cameras = imported_cameras};
         if (source.mRootNode != nullptr) {
            if (auto root = node(catalog, source, *source.mRootNode, *imported_meshes, scene); !root) {
               return std::unexpected(root.error());
            }
         }
         const auto handle = scene.handle;
         if (auto added = catalog.scenes.add(std::move(scene)); !added) { return std::unexpected(added.error()); }
         return handle;
      }

   } // namespace

   std::expected<SceneHandle, Error> AssetSystem::addScene(ObjectName name) {
      auto scene = Scene{.handle = makeCounterHandle<SceneHandle>(), .name = std::move(name)};
      const auto handle = scene.handle;
      if (auto added = catalog_.scenes.add(std::move(scene)); !added) { return std::unexpected(added.error()); }
      return handle;
   }

   std::expected<SceneHandle, Error> AssetSystem::loadScene(const std::filesystem::path &source) {
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

   bool AssetSystem::containsScene(SceneHandle scene) const { return catalog_.scenes.contains(scene); }

   NameExpected AssetSystem::sceneName(SceneHandle scene) const { return sceneField(catalog_, scene, &Scene::name); }

   CountExpected AssetSystem::sceneNodeCount(SceneHandle scene) const { return sizeOf(sceneNodes(scene)); }

   CountExpected AssetSystem::sceneMeshCount(SceneHandle scene) const { return sizeOf(sceneMeshes(scene)); }

   CountExpected AssetSystem::sceneMaterialCount(SceneHandle scene) const {
      return sizeOf(sceneMaterials(scene));
   }

   CountExpected AssetSystem::sceneTextureCount(SceneHandle scene) const {
      return sizeOf(sceneTextures(scene));
   }

   CountExpected AssetSystem::sceneLightCount(SceneHandle scene) const { return sizeOf(sceneLights(scene)); }

   CountExpected AssetSystem::sceneCameraCount(SceneHandle scene) const { return sizeOf(sceneCameras(scene)); }

   std::expected<NodeHandle, Error> AssetSystem::sceneRootNode(SceneHandle scene) const {
      const auto root = sceneField(catalog_, scene, &Scene::tree);
      if (!root) { return std::unexpected(root.error()); }
      if (!root->root.valid()) { return std::unexpected(Error::missing_object); }
      return root->root;
   }

   VectorExpected<NodeHandle> AssetSystem::sceneNodes(SceneHandle scene) const {
      return sceneField(catalog_, scene, &Scene::nodes);
   }

   VectorExpected<MeshHandle> AssetSystem::sceneMeshes(SceneHandle scene) const {
      return sceneField(catalog_, scene, &Scene::meshes);
   }

   VectorExpected<MaterialHandle> AssetSystem::sceneMaterials(SceneHandle scene) const {
      return sceneField(catalog_, scene, &Scene::materials);
   }

   VectorExpected<TextureHandle> AssetSystem::sceneTextures(SceneHandle scene) const {
      return sceneField(catalog_, scene, &Scene::textures);
   }

   VectorExpected<LightHandle> AssetSystem::sceneLights(SceneHandle scene) const {
      return sceneField(catalog_, scene, &Scene::lights);
   }

   VectorExpected<CameraHandle> AssetSystem::sceneCameras(SceneHandle scene) const {
      return sceneField(catalog_, scene, &Scene::cameras);
   }

   std::expected<Vector<NodeHandle>, Error> AssetSystem::sceneNodeChildren(SceneHandle scene, NodeHandle node) const {
      const auto source = sceneWithNode(catalog_, scene, node);
      if (!source) { return std::unexpected(source.error()); }
      Vector<NodeHandle> result{};
      const auto [first, last] = (*source)->tree.children.equal_range(node);
      for (auto it = first; it != last; ++it) { result.push_back(it->second); }
      return result;
   }

   std::expected<std::optional<NodeHandle>, Error> AssetSystem::sceneNodeParent(SceneHandle scene,
                                                                                NodeHandle node) const {
      const auto source = sceneWithNode(catalog_, scene, node);
      if (!source) { return std::unexpected(source.error()); }
      const auto parent = (*source)->tree.parents.find(node);
      return parent == (*source)->tree.parents.end() ? std::optional<NodeHandle>{}
                                                     : std::optional<NodeHandle>{parent->second};
   }

   NameExpected AssetSystem::nodeName(NodeHandle node) const { return field(catalog_.nodes, node, &Node::name); }

   Expected<Transform> AssetSystem::nodeTransform(NodeHandle node) const {
      return field(catalog_.nodes, node, &Node::transform);
   }

   VectorExpected<MeshHandle> AssetSystem::nodeMeshes(NodeHandle node) const {
      return field(catalog_.nodes, node, &Node::meshes);
   }

   VectorExpected<MaterialHandle> AssetSystem::nodeMaterials(NodeHandle node) const {
      return field(catalog_.nodes, node, &Node::materials);
   }

   NameExpected AssetSystem::meshName(MeshHandle mesh) const { return field(catalog_.meshes, mesh, &Mesh::name); }

   Expected<VertexCount> AssetSystem::meshVertexCount(MeshHandle mesh) const {
      return field(catalog_.meshes, mesh, &Mesh::vertex_count);
   }

   Expected<IndexCount> AssetSystem::meshIndexCount(MeshHandle mesh) const {
      return field(catalog_.meshes, mesh, &Mesh::index_count);
   }

   Expected<MaterialHandle> AssetSystem::meshMaterial(MeshHandle mesh) const {
      return field(catalog_.meshes, mesh, &Mesh::material);
   }

   Expected<Bounds> AssetSystem::meshBounds(MeshHandle mesh) const {
      return field(catalog_.meshes, mesh, &Mesh::bounds);
   }

   NameExpected AssetSystem::materialName(MaterialHandle material) const {
      return field(catalog_.materials, material, &Material::name);
   }

   VectorExpected<TextureHandle> AssetSystem::materialTextures(MaterialHandle material) const {
      return field(catalog_.materials, material, &Material::textures);
   }

} // namespace vve::v4
