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
/// @brief Assimp-backed v4 asset import into handle-addressable descriptors.

export namespace vve::v4 {

   /// @brief Asset facade that owns imported descriptors and imports scenes through Assimp.
   class AssetSystem {
   public:
      /// @brief Returns the mutable object catalog for tests and future loaders.
      [[nodiscard]] ObjectCatalog &catalog();
      /// @brief Returns the read-only object catalog.
      [[nodiscard]] const ObjectCatalog &catalog() const;
      /// @brief Allocates the next counter handle.
      [[nodiscard]] Handle next();
      /// @brief Adds an empty scene descriptor and returns its handle.
      [[nodiscard]] std::expected<Handle, Error> addScene(ObjectName name);
      /// @brief Imports a scene file through Assimp and returns the v4 scene handle.
      [[nodiscard]] std::expected<Handle, Error> loadScene(const std::filesystem::path &source);

   private:
      ObjectCatalog catalog_{};                         ///< All loaded object descriptors.
   };

} // namespace vve::v4

namespace vve::v4 {

   namespace {

      /// @brief Maps common Assimp texture slots to v4 material semantics.
      static const std::map<aiTextureType, TextureSemantic> texture_semantics{
         {aiTextureType_DIFFUSE,           TextureSemantic::base_color},
         {aiTextureType_BASE_COLOR,        TextureSemantic::base_color},
         {aiTextureType_NORMALS,           TextureSemantic::normal},
         {aiTextureType_HEIGHT,            TextureSemantic::normal},
         {aiTextureType_DIFFUSE_ROUGHNESS, TextureSemantic::roughness},
         {aiTextureType_METALNESS,         TextureSemantic::metallic},
         {aiTextureType_EMISSIVE,          TextureSemantic::emissive},
         {aiTextureType_AMBIENT_OCCLUSION, TextureSemantic::occlusion},
         {aiTextureType_LIGHTMAP,          TextureSemantic::occlusion}};

      /// @brief Imported material handles plus texture handles referenced by those materials.
      struct ImportedMaterials {
         Vector<Handle> materials{}; ///< Imported material descriptor handles.
         Vector<Handle> textures{};  ///< Imported texture descriptor handles.
      };

      /// @brief Returns a stable filesystem spelling when possible.
      [[nodiscard]] std::filesystem::path normalizePath(const std::filesystem::path &path) {
         std::error_code error_code{};
         const auto canonical = std::filesystem::weakly_canonical(path, error_code);
         return error_code ? path.lexically_normal() : canonical;
      }

      /// @brief Converts an Assimp vector to the shared engine vector type.
      [[nodiscard]] Vec3 toVec3(const aiVector3D &value) {
         return Vec3(static_cast<Scalar>(value.x), static_cast<Scalar>(value.y), static_cast<Scalar>(value.z));
      }

      /// @brief Converts an Assimp color to the shared engine vector type.
      [[nodiscard]] Vec3 toVec3(const aiColor3D &value) {
         return Vec3(static_cast<Scalar>(value.r), static_cast<Scalar>(value.g), static_cast<Scalar>(value.b));
      }

      /// @brief Converts an Assimp quaternion to the shared engine quaternion type.
      [[nodiscard]] Quat toQuat(const aiQuaternion &value) {
         return Quat(static_cast<Scalar>(value.w), static_cast<Scalar>(value.x), static_cast<Scalar>(value.y),
                     static_cast<Scalar>(value.z));
      }

      /// @brief Converts an Assimp matrix into the shared transform component.
      [[nodiscard]] Transform toTransform(const aiMatrix4x4 &matrix) {
         aiVector3D scale{};
         aiQuaternion rotation{};
         aiVector3D translation{};
         matrix.Decompose(scale, rotation, translation);
         return Transform{.translation = Position{.value = toVec3(translation)},
                          .rotation = Rotation{.value = toQuat(rotation)},
                          .scale = Scale{.value = toVec3(scale)}};
      }

      /// @brief Returns a readable Assimp name or a generated fallback.
      [[nodiscard]] std::string readableName(const aiString &name, std::string fallback) {
         return name.length > 0 ? std::string{name.C_Str()} : std::move(fallback);
      }

      /// @brief Converts Assimp light categories to the v4 descriptor category.
      [[nodiscard]] LightKind mapLightKind(const aiLightSourceType type) {
         switch (type) {
         case aiLightSource_DIRECTIONAL:
            return LightKind::directional;
         case aiLightSource_POINT:
            return LightKind::point;
         case aiLightSource_SPOT:
            return LightKind::spot;
         default:
            return LightKind::unknown;
         }
      }

      /// @brief Returns the first non-black Assimp light color, or white.
      [[nodiscard]] LinearColor visibleColor(const aiLight &light) {
         const auto diffuse = toVec3(light.mColorDiffuse);
         if (math::lengthSquared(diffuse) > Scalar{0}) {
            return LinearColor{.value = diffuse};
         }
         const auto specular = toVec3(light.mColorSpecular);
         if (math::lengthSquared(specular) > Scalar{0}) {
            return LinearColor{.value = specular};
         }
         return LinearColor{.value = oneVec3()};
      }

      /// @brief Resolves material texture references relative to the imported scene file.
      [[nodiscard]] std::filesystem::path texturePath(const aiString &path, const std::filesystem::path &scene_dir) {
         const auto raw = std::filesystem::path(path.C_Str());
         if (raw.empty() || raw.is_absolute() || raw.string().starts_with('*')) { return raw; }
         return normalizePath(scene_dir / raw);
      }

      /// @brief Imports one texture reference and reuses duplicate path descriptors.
      [[nodiscard]] std::expected<Handle, Error> importTexture(AssetSystem &assets,
                                                               const aiScene &scene,
                                                               const std::filesystem::path &source,
                                                               std::map<std::string, Handle> &textures,
                                                               Vector<Handle> &scene_textures) {
         const auto key = source.string();
         if (const auto existing = textures.find(key); existing != textures.end()) { return existing->second; }

         auto texture = TextureDescriptor{.handle = assets.next(),
                                          .name = ObjectName{.value = source.filename().string()},
                                          .source = source};
         if (key.starts_with('*')) {
            const auto index = static_cast<std::size_t>(std::strtoull(key.c_str() + 1, nullptr, 10));
            if (index < scene.mNumTextures && scene.mTextures[index] != nullptr) {
               texture.extent = PixelExtent{.width = scene.mTextures[index]->mWidth,
                                            .height = scene.mTextures[index]->mHeight};
            }
         }

         if (auto added = assets.catalog().textures.add(texture); !added) { return std::unexpected(added.error()); }
         textures.emplace(key, texture.handle);
         scene_textures.push_back(texture.handle);
         return texture.handle;
      }

      /// @brief Imports all materials and material texture references.
      [[nodiscard]] std::expected<ImportedMaterials, Error> importMaterials(AssetSystem &assets,
                                                                           const aiScene &scene,
                                                                           const std::filesystem::path &scene_dir) {
         auto imported = ImportedMaterials{.materials = Vector<Handle>(scene.mNumMaterials)};
         std::map<std::string, Handle> textures{};
         for (unsigned material_index = 0; material_index < scene.mNumMaterials; ++material_index) {
            const auto *source = scene.mMaterials[material_index];
            aiString name{};
            if (source != nullptr) { source->Get(AI_MATKEY_NAME, name); }

            auto material = MaterialDescriptor{
               .handle = assets.next(),
               .name = ObjectName{.value = readableName(name, "Material_" + std::to_string(material_index))}};

            if (source != nullptr) {
               for (const auto &[type, semantic] : texture_semantics) {
                  for (unsigned slot = 0; slot < source->GetTextureCount(type); ++slot) {
                     aiString path{};
                     if (source->GetTexture(type, slot, &path) != AI_SUCCESS) { continue; }
                     const auto texture = importTexture(assets, scene, texturePath(path, scene_dir), textures,
                                                        imported.textures);
                     if (!texture) { return std::unexpected(texture.error()); }
                     material.textures.push_back(TextureBinding{.texture = *texture, .semantic = semantic});
                  }
               }
            }

            if (auto added = assets.catalog().materials.add(material); !added) {
               return std::unexpected(added.error());
            }
            imported.materials[material_index] = material.handle;
         }
         return imported;
      }

      /// @brief Computes simple vertex-count, index-count, material, and bounds descriptors.
      [[nodiscard]] std::expected<Vector<Handle>, Error> importMeshes(AssetSystem &assets,
                                                                      const aiScene &scene,
                                                                      const Vector<Handle> &materials) {
         Vector<Handle> meshes(scene.mNumMeshes);
         for (unsigned mesh_index = 0; mesh_index < scene.mNumMeshes; ++mesh_index) {
            const auto *source = scene.mMeshes[mesh_index];
            if (source == nullptr) { continue; }

            Bounds bounds{};
            if (source->mNumVertices > 0 && source->mVertices != nullptr) {
               bounds.valid = true;
               bounds.minimum.value = toVec3(source->mVertices[0]);
               bounds.maximum.value = bounds.minimum.value;
               for (unsigned vertex = 1; vertex < source->mNumVertices; ++vertex) {
                  const auto point = toVec3(source->mVertices[vertex]);
                  bounds.minimum.value = math::min(bounds.minimum.value, point);
                  bounds.maximum.value = math::max(bounds.maximum.value, point);
               }
            }

            std::uint64_t index_count = 0;
            for (unsigned face = 0; face < source->mNumFaces; ++face) {
               index_count += source->mFaces[face].mNumIndices;
            }

            const auto material = source->mMaterialIndex < materials.size() ? materials[source->mMaterialIndex]
                                                                            : Handle{};
            auto mesh = MeshDescriptor{.handle = assets.next(),
                                       .name = ObjectName{
                                          .value = readableName(source->mName,
                                                                "Mesh_" + std::to_string(mesh_index))},
                                       .vertex_count = VertexCount{.value = source->mNumVertices},
                                       .index_count = IndexCount{.value = index_count},
                                       .material = material,
                                       .bounds = bounds};
            if (auto added = assets.catalog().meshes.add(mesh); !added) { return std::unexpected(added.error()); }
            meshes[mesh_index] = mesh.handle;
         }
         return meshes;
      }

      /// @brief Imports one scene-graph node and its children into descriptor maps and topology.
      [[nodiscard]] std::expected<Handle, Error> importNode(AssetSystem &assets,
                                                            const aiScene &source_scene,
                                                            const aiNode &source,
                                                            const Vector<Handle> &meshes,
                                                            SceneDescriptor &scene,
                                                            Handle parent = {}) {
         auto node = NodeDescriptor{.handle = assets.next(),
                                    .name = ObjectName{
                                       .value = readableName(source.mName,
                                                             "Node_" + std::to_string(scene.nodes.size()))},
                                    .transform = toTransform(source.mTransformation)};
         for (unsigned mesh_slot = 0; mesh_slot < source.mNumMeshes; ++mesh_slot) {
            const auto mesh_index = source.mMeshes[mesh_slot];
            if (mesh_index >= meshes.size() || !meshes[mesh_index].valid()) { continue; }
            const auto *mesh = source_scene.mMeshes[mesh_index];
            const auto material = mesh != nullptr && mesh->mMaterialIndex < scene.materials.size()
                                    ? scene.materials[mesh->mMaterialIndex]
                                    : Handle{};
            node.meshes.push_back(MeshUse{.mesh = meshes[mesh_index], .material = material});
         }

         const auto handle = node.handle;
         if (auto added = assets.catalog().nodes.add(std::move(node)); !added) {
            return std::unexpected(added.error());
         }
         if (parent.valid()) {
            scene.tree.addChild(parent, handle);
         } else {
            scene.tree.root = handle;
         }
         scene.nodes.push_back(handle);

         for (unsigned child = 0; child < source.mNumChildren; ++child) {
            if (source.mChildren[child] == nullptr) { continue; }
            if (auto imported = importNode(assets, source_scene, *source.mChildren[child], meshes, scene, handle);
                !imported) {
               return std::unexpected(imported.error());
            }
         }
         return handle;
      }

      /// @brief Imports Assimp lights into v4 light descriptors.
      [[nodiscard]] std::expected<Vector<Handle>, Error> importLights(AssetSystem &assets, const aiScene &scene) {
         Vector<Handle> lights{};
         lights.reserve(scene.mNumLights);
         for (unsigned light_index = 0; light_index < scene.mNumLights; ++light_index) {
            const auto *source = scene.mLights[light_index];
            if (source == nullptr) { continue; }
            const auto color = visibleColor(*source);
            const auto intensity = math::max(math::max(std::abs(color.value.x), std::abs(color.value.y)),
                                             math::max(std::abs(color.value.z), one()));
            auto light = LightDescriptor{
               .handle = assets.next(),
               .name = ObjectName{.value = readableName(source->mName, "Light_" + std::to_string(light_index))},
               .kind = mapLightKind(source->mType),
               .position = Position{.value = toVec3(source->mPosition)},
               .direction = Direction{.value = toVec3(source->mDirection)},
               .color = color,
               .intensity = LightIntensity{.value = intensity}};
            if (auto added = assets.catalog().lights.add(light); !added) { return std::unexpected(added.error()); }
            lights.push_back(light.handle);
         }
         return lights;
      }

      /// @brief Imports Assimp cameras into v4 camera descriptors.
      [[nodiscard]] std::expected<Vector<Handle>, Error> importCameras(AssetSystem &assets, const aiScene &scene) {
         Vector<Handle> cameras{};
         cameras.reserve(scene.mNumCameras);
         for (unsigned camera_index = 0; camera_index < scene.mNumCameras; ++camera_index) {
            const auto *source = scene.mCameras[camera_index];
            if (source == nullptr) { continue; }
            const auto aspect = math::max(static_cast<Scalar>(source->mAspect), one());
            const auto horizontal = math::max(static_cast<Scalar>(source->mHorizontalFOV), Scalar{0.001F});
            const auto vertical = Scalar{2} * std::atan(std::tan(horizontal * Scalar{0.5F}) / aspect);
            auto camera = CameraDescriptor{.handle = assets.next(),
                                           .name = ObjectName{
                                              .value = readableName(source->mName,
                                                                    "Camera_" + std::to_string(camera_index))},
                                           .position = Position{.value = toVec3(source->mPosition)},
                                           .forward = Direction{.value = toVec3(source->mLookAt)},
                                           .fov_y = FovY{.radians = vertical},
                                           .clip = ClipPlanes{
                                              .near_plane = math::max(static_cast<Scalar>(source->mClipPlaneNear),
                                                                      Scalar{0.001F}),
                                              .far_plane = math::max(static_cast<Scalar>(source->mClipPlaneFar),
                                                                     Scalar{1})}};
            if (auto added = assets.catalog().cameras.add(camera); !added) { return std::unexpected(added.error()); }
            cameras.push_back(camera.handle);
         }
         return cameras;
      }

      /// @brief Converts an Assimp scene into v4 descriptors.
      [[nodiscard]] std::expected<Handle, Error> importScene(AssetSystem &assets,
                                                             const aiScene &source,
                                                             const std::filesystem::path &path) {
         const auto scene_dir = path.parent_path();
         const auto material_handles = importMaterials(assets, source, scene_dir);
         if (!material_handles) { return std::unexpected(material_handles.error()); }
         const auto mesh_handles = importMeshes(assets, source, material_handles->materials);
         if (!mesh_handles) { return std::unexpected(mesh_handles.error()); }
         const auto light_handles = importLights(assets, source);
         if (!light_handles) { return std::unexpected(light_handles.error()); }
         const auto camera_handles = importCameras(assets, source);
         if (!camera_handles) { return std::unexpected(camera_handles.error()); }

         auto scene = SceneDescriptor{.handle = assets.next(),
                                      .name = ObjectName{.value = path.filename().string()},
                                      .meshes = *mesh_handles,
                                      .materials = material_handles->materials,
                                      .textures = material_handles->textures,
                                      .lights = *light_handles,
                                      .cameras = *camera_handles};
         if (source.mRootNode != nullptr) {
            if (auto root = importNode(assets, source, *source.mRootNode, *mesh_handles, scene); !root) {
               return std::unexpected(root.error());
            }
         }

         const auto handle = scene.handle;
         if (auto added = assets.catalog().scenes.add(std::move(scene)); !added) {
            return std::unexpected(added.error());
         }
         return handle;
      }

   } // namespace

   ObjectCatalog &AssetSystem::catalog() { return catalog_; }

   const ObjectCatalog &AssetSystem::catalog() const { return catalog_; }

   Handle AssetSystem::next() { return makeCounterHandle(); }

   std::expected<Handle, Error> AssetSystem::addScene(ObjectName name) {
      const auto handle = next();
      auto scene = SceneDescriptor{.handle = handle, .name = std::move(name)};
      if (auto added = catalog_.scenes.add(std::move(scene)); !added) { return std::unexpected(added.error()); }
      return handle;
   }

   std::expected<Handle, Error> AssetSystem::loadScene(const std::filesystem::path &source) {
      if (source.empty()) { return std::unexpected(Error::invalid_argument); }
      const auto path = normalizePath(source);
      Assimp::Importer importer{};
      constexpr auto flags = aiProcess_Triangulate | aiProcess_JoinIdenticalVertices |
                             aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace |
                             aiProcess_ImproveCacheLocality;
      const aiScene *scene = importer.ReadFile(path.string(), flags);
      if (scene == nullptr || scene->mRootNode == nullptr) { return std::unexpected(Error::asset_import_failed); }
      return importScene(*this, *scene, path);
   }

} // namespace vve::v4
