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

namespace vve::v4 {

   /// @brief Internal scene-tree topology for imported asset nodes.
   struct SceneTree {
      NodeHandle root{}; ///< Root node handle.
      std::unordered_multimap<NodeHandle, NodeHandle, HandleHash<NodeHandle>> children{}; ///< Parent to children.
      std::unordered_map<NodeHandle, NodeHandle, HandleHash<NodeHandle>> parents{}; ///< Child to parent.

      /// @brief Adds one parent-to-child tree edge.
      void addChild(NodeHandle parent, NodeHandle child) {
         if (const auto old_parent = parents.find(child); old_parent != parents.end()) {
            removeChildEdge(old_parent->second, child);
         }
         children.emplace(parent, child);
         parents[child] = parent;
      }

   private:
      /// @brief Removes all matching parent-to-child edges.
      void removeChildEdge(NodeHandle parent, NodeHandle child) {
         auto [first, last] = children.equal_range(parent);
         for (auto it = first; it != last;) { it = it->second == child ? children.erase(it) : std::next(it); }
      }
   };

   using Tree = SceneTree; ///< Internal scene-tree topology.

   /// @brief Internal material texture slot meaning for imported assets.
   enum class TextureSemantic {
      unknown,    ///< Unclassified texture use.
      base_color, ///< Color/albedo texture.
      normal,     ///< Tangent-space normal texture.
      roughness,  ///< Roughness texture.
      metallic,   ///< Metallic texture.
      emissive,   ///< Emissive texture.
      occlusion   ///< Ambient-occlusion texture.
   };

   /// @brief Internal imported-light shape classification.
   enum class LightKind {
      unknown,     ///< Unclassified light.
      directional, ///< Direction-only light such as the sun.
      point,       ///< Point light with position.
      spot         ///< Spot light with position and direction.
   };

   /// @brief Internal material reference to one texture descriptor.
   struct TextureBinding {
      TextureHandle texture{};                            ///< Referenced texture handle.
      TextureSemantic semantic{TextureSemantic::unknown}; ///< Intended material slot.
      std::uint32_t uv_set{0};                            ///< UV channel used by the texture.
   };

   /// @brief Internal scene node reference to renderable geometry and material.
   struct MeshUse {
      MeshHandle mesh{};         ///< Referenced mesh handle.
      MaterialHandle material{}; ///< Referenced material handle.
   };

   /// @brief Internal scene graph node descriptor stored by handle in the object catalog.
   struct NodeDescriptor {
      using HandleType = NodeHandle; ///< Descriptor handle type.
      NodeHandle handle{};           ///< Stable node handle.
      ObjectName name{};             ///< Human-readable node name.
      Transform transform{};         ///< Local transform.
      Vector<MeshUse> meshes{};      ///< Mesh/material pairs attached to this node.
   };

   /// @brief Internal mesh geometry descriptor; actual vertex buffers are implementation work.
   struct MeshDescriptor {
      using HandleType = MeshHandle; ///< Descriptor handle type.
      MeshHandle handle{};           ///< Stable mesh handle.
      ObjectName name{};             ///< Human-readable mesh name.
      VertexCount vertex_count{};    ///< Number of vertices in source geometry.
      IndexCount index_count{};      ///< Number of indices in source geometry.
      MaterialHandle material{};     ///< Default material handle.
      Bounds bounds{};               ///< Object-space bounds.
   };

   /// @brief Internal material descriptor referencing textures by handle.
   struct MaterialDescriptor {
      using HandleType = MaterialHandle; ///< Descriptor handle type.
      MaterialHandle handle{};           ///< Stable material handle.
      ObjectName name{};                 ///< Human-readable material name.
      Vector<TextureBinding> textures{}; ///< Texture slots used by this material.
   };

   /// @brief Internal texture descriptor; pixel storage and GPU upload are implementation work.
   struct TextureDescriptor {
      using HandleType = TextureHandle;    ///< Descriptor handle type.
      TextureHandle handle{};              ///< Stable texture handle.
      ObjectName name{};                   ///< Human-readable texture name.
      std::filesystem::path source{};      ///< Source file path or logical asset path.
      PixelExtent extent{};                ///< Source dimensions in pixels.
      TextureChannelCount channels{};      ///< Source channel count.
   };

   /// @brief Internal imported-light descriptor.
   struct LightDescriptor {
      using HandleType = LightHandle;     ///< Descriptor handle type.
      LightHandle handle{};               ///< Stable light handle.
      ObjectName name{};                  ///< Human-readable light name.
      LightKind kind{LightKind::unknown}; ///< Light shape.
      Position position{};                ///< Light position for point/spot lights.
      Direction direction{};              ///< Light direction for directional/spot lights.
      LinearColor color{};                ///< Linear light color.
      LightIntensity intensity{};         ///< Relative light intensity.
   };

   /// @brief Internal imported-camera descriptor.
   struct CameraDescriptor {
      using HandleType = CameraHandle; ///< Descriptor handle type.
      CameraHandle handle{};           ///< Stable camera handle.
      ObjectName name{};               ///< Human-readable camera name.
      Position position{};             ///< Camera position.
      Direction forward{};             ///< Camera forward direction.
      FovY fov_y{};                    ///< Vertical field of view.
      ClipPlanes clip{};               ///< Near and far clipping planes.
   };

   /// @brief Scene descriptor stores handles to objects kept in the v4 object catalog.
   struct SceneDescriptor {
      using HandleType = SceneHandle;     ///< Descriptor handle type.
      SceneHandle handle{};               ///< Stable scene handle.
      ObjectName name{};                  ///< Human-readable scene name.
      Tree tree{};                        ///< Scene hierarchy; nodes do not store child vectors.
      Vector<NodeHandle> nodes{};         ///< All node handles in the scene.
      Vector<MeshHandle> meshes{};        ///< Mesh handles used by the scene.
      Vector<MaterialHandle> materials{}; ///< Material handles used by the scene.
      Vector<TextureHandle> textures{};   ///< Texture handles used by the scene.
      Vector<LightHandle> lights{};       ///< Light handles used by the scene.
      Vector<CameraHandle> cameras{};     ///< Camera handles used by the scene.
   };

   /// @brief Simple v4 descriptor table keyed by each descriptor's typed handle.
   template <typename TDescriptor> class DescriptorMap {
   public:
      using HandleType = typename TDescriptor::HandleType; ///< Strong handle accepted by this map.

      [[nodiscard]] std::expected<void, Error> add(TDescriptor descriptor) {
         if (!descriptor.handle.valid()) { return std::unexpected(Error::invalid_handle); }
         const auto [_, inserted] = descriptors_.emplace(descriptor.handle, std::move(descriptor));
         if (!inserted) { return std::unexpected(Error::duplicate_object); }
         return {};
      }

      [[nodiscard]] const TDescriptor *find(HandleType handle) const {
         const auto it = descriptors_.find(handle);
         return it == descriptors_.end() ? nullptr : std::addressof(it->second);
      }

      [[nodiscard]] TDescriptor *find(HandleType handle) {
         const auto it = descriptors_.find(handle);
         return it == descriptors_.end() ? nullptr : std::addressof(it->second);
      }

      [[nodiscard]] bool contains(HandleType handle) const { return descriptors_.contains(handle); }

      [[nodiscard]] std::expected<void, Error> remove(HandleType handle) {
         if (!handle.valid()) { return std::unexpected(Error::invalid_handle); }
         if (descriptors_.erase(handle) == 0) { return std::unexpected(Error::missing_object); }
         return {};
      }

      [[nodiscard]] std::size_t size() const { return descriptors_.size(); }
      [[nodiscard]] const std::map<HandleType, TDescriptor> &all() const { return descriptors_; }

   private:
      std::map<HandleType, TDescriptor> descriptors_{}; ///< Ordered descriptor storage.
   };

   /// @brief Central v4 imported-object catalog; every loaded object is found by typed handle.
   struct ObjectCatalog {
      DescriptorMap<SceneDescriptor> scenes{};       ///< Scenes by handle.
      DescriptorMap<NodeDescriptor> nodes{};         ///< Nodes by handle.
      DescriptorMap<MeshDescriptor> meshes{};        ///< Meshes by handle.
      DescriptorMap<MaterialDescriptor> materials{}; ///< Materials by handle.
      DescriptorMap<TextureDescriptor> textures{};   ///< Textures by handle.
      DescriptorMap<LightDescriptor> lights{};       ///< Lights by handle.
      DescriptorMap<CameraDescriptor> cameras{};     ///< Cameras by handle.
   };

} // namespace vve::v4

export namespace vve::v4 {

   /// @brief Asset facade that owns imported descriptors and imports scenes through Assimp.
   class AssetSystem {
   public:
      /// @brief Adds an empty scene descriptor and returns its handle.
      [[nodiscard]] std::expected<SceneHandle, Error> addScene(ObjectName name);
      /// @brief Imports a scene file through Assimp and returns the v4 scene handle.
      [[nodiscard]] std::expected<SceneHandle, Error> loadScene(const std::filesystem::path &source);
      /// @brief Returns whether an imported scene exists.
      [[nodiscard]] bool containsScene(SceneHandle scene) const;
      /// @brief Returns the imported scene name.
      [[nodiscard]] std::expected<ObjectName, Error> sceneName(SceneHandle scene) const;
      /// @brief Returns the number of nodes in an imported scene.
      [[nodiscard]] std::expected<std::size_t, Error> sceneNodeCount(SceneHandle scene) const;
      /// @brief Returns the number of meshes referenced by an imported scene.
      [[nodiscard]] std::expected<std::size_t, Error> sceneMeshCount(SceneHandle scene) const;
      /// @brief Returns the number of materials referenced by an imported scene.
      [[nodiscard]] std::expected<std::size_t, Error> sceneMaterialCount(SceneHandle scene) const;
      /// @brief Returns the number of textures referenced by an imported scene.
      [[nodiscard]] std::expected<std::size_t, Error> sceneTextureCount(SceneHandle scene) const;
      /// @brief Returns the number of lights referenced by an imported scene.
      [[nodiscard]] std::expected<std::size_t, Error> sceneLightCount(SceneHandle scene) const;
      /// @brief Returns the number of cameras referenced by an imported scene.
      [[nodiscard]] std::expected<std::size_t, Error> sceneCameraCount(SceneHandle scene) const;

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
         Vector<MaterialHandle> materials{}; ///< Imported material descriptor handles.
         Vector<TextureHandle> textures{};   ///< Imported texture descriptor handles.
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
      [[nodiscard]] std::expected<TextureHandle, Error> importTexture(ObjectCatalog &catalog,
                                                                      const aiScene &scene,
                                                                      const std::filesystem::path &source,
                                                                      std::map<std::string, TextureHandle> &textures,
                                                                      Vector<TextureHandle> &scene_textures) {
         const auto key = source.string();
         if (const auto existing = textures.find(key); existing != textures.end()) { return existing->second; }

         auto texture = TextureDescriptor{.handle = makeCounterHandle<TextureHandle>(),
                                          .name = ObjectName{.value = source.filename().string()},
                                          .source = source};
         if (key.starts_with('*')) {
            const auto index = static_cast<std::size_t>(std::strtoull(key.c_str() + 1, nullptr, 10));
            if (index < scene.mNumTextures && scene.mTextures[index] != nullptr) {
               texture.extent = PixelExtent{.width = scene.mTextures[index]->mWidth,
                                            .height = scene.mTextures[index]->mHeight};
            }
         }

         if (auto added = catalog.textures.add(texture); !added) { return std::unexpected(added.error()); }
         textures.emplace(key, texture.handle);
         scene_textures.push_back(texture.handle);
         return texture.handle;
      }

      /// @brief Imports all materials and material texture references.
      [[nodiscard]] std::expected<ImportedMaterials, Error> importMaterials(ObjectCatalog &catalog,
                                                                           const aiScene &scene,
                                                                           const std::filesystem::path &scene_dir) {
         auto imported = ImportedMaterials{.materials = Vector<MaterialHandle>(scene.mNumMaterials)};
         std::map<std::string, TextureHandle> textures{};
         for (unsigned material_index = 0; material_index < scene.mNumMaterials; ++material_index) {
            const auto *source = scene.mMaterials[material_index];
            aiString name{};
            if (source != nullptr) { source->Get(AI_MATKEY_NAME, name); }

            auto material = MaterialDescriptor{
               .handle = makeCounterHandle<MaterialHandle>(),
               .name = ObjectName{.value = readableName(name, "Material_" + std::to_string(material_index))}};

            if (source != nullptr) {
               for (const auto &[type, semantic] : texture_semantics) {
                  for (unsigned slot = 0; slot < source->GetTextureCount(type); ++slot) {
                     aiString path{};
                     if (source->GetTexture(type, slot, &path) != AI_SUCCESS) { continue; }
                     const auto texture = importTexture(catalog, scene, texturePath(path, scene_dir), textures,
                                                        imported.textures);
                     if (!texture) { return std::unexpected(texture.error()); }
                     material.textures.push_back(TextureBinding{.texture = *texture, .semantic = semantic});
                  }
               }
            }

            if (auto added = catalog.materials.add(material); !added) {
               return std::unexpected(added.error());
            }
            imported.materials[material_index] = material.handle;
         }
         return imported;
      }

      /// @brief Computes simple vertex-count, index-count, material, and bounds descriptors.
      [[nodiscard]] std::expected<Vector<MeshHandle>, Error> importMeshes(ObjectCatalog &catalog,
                                                                          const aiScene &scene,
                                                                          const Vector<MaterialHandle> &materials) {
         Vector<MeshHandle> meshes(scene.mNumMeshes);
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
                                                                            : MaterialHandle{};
            auto mesh = MeshDescriptor{.handle = makeCounterHandle<MeshHandle>(),
                                       .name = ObjectName{
                                          .value = readableName(source->mName,
                                                                "Mesh_" + std::to_string(mesh_index))},
                                       .vertex_count = VertexCount{.value = source->mNumVertices},
                                       .index_count = IndexCount{.value = index_count},
                                       .material = material,
                                       .bounds = bounds};
            if (auto added = catalog.meshes.add(mesh); !added) { return std::unexpected(added.error()); }
            meshes[mesh_index] = mesh.handle;
         }
         return meshes;
      }

      /// @brief Imports one scene-graph node and its children into descriptor maps and topology.
      [[nodiscard]] std::expected<NodeHandle, Error> importNode(ObjectCatalog &catalog,
                                                                const aiScene &source_scene,
                                                                const aiNode &source,
                                                                const Vector<MeshHandle> &meshes,
                                                                SceneDescriptor &scene,
                                                                NodeHandle parent = {}) {
         auto node = NodeDescriptor{.handle = makeCounterHandle<NodeHandle>(),
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
                                    : MaterialHandle{};
            node.meshes.push_back(MeshUse{.mesh = meshes[mesh_index], .material = material});
         }

         const auto handle = node.handle;
         if (auto added = catalog.nodes.add(std::move(node)); !added) {
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
            if (auto imported = importNode(catalog, source_scene, *source.mChildren[child], meshes, scene, handle);
                !imported) {
               return std::unexpected(imported.error());
            }
         }
         return handle;
      }

      /// @brief Imports Assimp lights into v4 light descriptors.
      [[nodiscard]] std::expected<Vector<LightHandle>, Error> importLights(ObjectCatalog &catalog, const aiScene &scene) {
         Vector<LightHandle> lights{};
         lights.reserve(scene.mNumLights);
         for (unsigned light_index = 0; light_index < scene.mNumLights; ++light_index) {
            const auto *source = scene.mLights[light_index];
            if (source == nullptr) { continue; }
            const auto color = visibleColor(*source);
            const auto intensity = math::max(math::max(std::abs(color.value.x), std::abs(color.value.y)),
                                             math::max(std::abs(color.value.z), one()));
            auto light = LightDescriptor{
               .handle = makeCounterHandle<LightHandle>(),
               .name = ObjectName{.value = readableName(source->mName, "Light_" + std::to_string(light_index))},
               .kind = mapLightKind(source->mType),
               .position = Position{.value = toVec3(source->mPosition)},
               .direction = Direction{.value = toVec3(source->mDirection)},
               .color = color,
               .intensity = LightIntensity{.value = intensity}};
            if (auto added = catalog.lights.add(light); !added) { return std::unexpected(added.error()); }
            lights.push_back(light.handle);
         }
         return lights;
      }

      /// @brief Imports Assimp cameras into v4 camera descriptors.
      [[nodiscard]] std::expected<Vector<CameraHandle>, Error> importCameras(ObjectCatalog &catalog,
                                                                             const aiScene &scene) {
         Vector<CameraHandle> cameras{};
         cameras.reserve(scene.mNumCameras);
         for (unsigned camera_index = 0; camera_index < scene.mNumCameras; ++camera_index) {
            const auto *source = scene.mCameras[camera_index];
            if (source == nullptr) { continue; }
            const auto aspect = math::max(static_cast<Scalar>(source->mAspect), one());
            const auto horizontal = math::max(static_cast<Scalar>(source->mHorizontalFOV), Scalar{0.001F});
            const auto vertical = Scalar{2} * std::atan(std::tan(horizontal * Scalar{0.5F}) / aspect);
            auto camera = CameraDescriptor{.handle = makeCounterHandle<CameraHandle>(),
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
            if (auto added = catalog.cameras.add(camera); !added) { return std::unexpected(added.error()); }
            cameras.push_back(camera.handle);
         }
         return cameras;
      }

      /// @brief Converts an Assimp scene into v4 descriptors.
      [[nodiscard]] std::expected<SceneHandle, Error> importScene(ObjectCatalog &catalog,
                                                                  const aiScene &source,
                                                                  const std::filesystem::path &path) {
         const auto scene_dir = path.parent_path();
         const auto material_handles = importMaterials(catalog, source, scene_dir);
         if (!material_handles) { return std::unexpected(material_handles.error()); }
         const auto mesh_handles = importMeshes(catalog, source, material_handles->materials);
         if (!mesh_handles) { return std::unexpected(mesh_handles.error()); }
         const auto light_handles = importLights(catalog, source);
         if (!light_handles) { return std::unexpected(light_handles.error()); }
         const auto camera_handles = importCameras(catalog, source);
         if (!camera_handles) { return std::unexpected(camera_handles.error()); }

         auto scene = SceneDescriptor{.handle = makeCounterHandle<SceneHandle>(),
                                      .name = ObjectName{.value = path.filename().string()},
                                      .meshes = *mesh_handles,
                                      .materials = material_handles->materials,
                                      .textures = material_handles->textures,
                                      .lights = *light_handles,
                                      .cameras = *camera_handles};
         if (source.mRootNode != nullptr) {
            if (auto root = importNode(catalog, source, *source.mRootNode, *mesh_handles, scene); !root) {
               return std::unexpected(root.error());
            }
         }

         const auto handle = scene.handle;
         if (auto added = catalog.scenes.add(std::move(scene)); !added) {
            return std::unexpected(added.error());
         }
         return handle;
      }

   } // namespace

   bool AssetSystem::containsScene(SceneHandle scene) const { return catalog_.scenes.contains(scene); }

   std::expected<ObjectName, Error> AssetSystem::sceneName(SceneHandle scene) const {
      const auto *descriptor = catalog_.scenes.find(scene);
      if (descriptor == nullptr) { return std::unexpected(Error::missing_object); }
      return descriptor->name;
   }

   std::expected<std::size_t, Error> AssetSystem::sceneNodeCount(SceneHandle scene) const {
      const auto *descriptor = catalog_.scenes.find(scene);
      if (descriptor == nullptr) { return std::unexpected(Error::missing_object); }
      return descriptor->nodes.size();
   }

   std::expected<std::size_t, Error> AssetSystem::sceneMeshCount(SceneHandle scene) const {
      const auto *descriptor = catalog_.scenes.find(scene);
      if (descriptor == nullptr) { return std::unexpected(Error::missing_object); }
      return descriptor->meshes.size();
   }

   std::expected<std::size_t, Error> AssetSystem::sceneMaterialCount(SceneHandle scene) const {
      const auto *descriptor = catalog_.scenes.find(scene);
      if (descriptor == nullptr) { return std::unexpected(Error::missing_object); }
      return descriptor->materials.size();
   }

   std::expected<std::size_t, Error> AssetSystem::sceneTextureCount(SceneHandle scene) const {
      const auto *descriptor = catalog_.scenes.find(scene);
      if (descriptor == nullptr) { return std::unexpected(Error::missing_object); }
      return descriptor->textures.size();
   }

   std::expected<std::size_t, Error> AssetSystem::sceneLightCount(SceneHandle scene) const {
      const auto *descriptor = catalog_.scenes.find(scene);
      if (descriptor == nullptr) { return std::unexpected(Error::missing_object); }
      return descriptor->lights.size();
   }

   std::expected<std::size_t, Error> AssetSystem::sceneCameraCount(SceneHandle scene) const {
      const auto *descriptor = catalog_.scenes.find(scene);
      if (descriptor == nullptr) { return std::unexpected(Error::missing_object); }
      return descriptor->cameras.size();
   }

   std::expected<SceneHandle, Error> AssetSystem::addScene(ObjectName name) {
      const auto handle = makeCounterHandle<SceneHandle>();
      auto scene = SceneDescriptor{.handle = handle, .name = std::move(name)};
      if (auto added = catalog_.scenes.add(std::move(scene)); !added) { return std::unexpected(added.error()); }
      return handle;
   }

   std::expected<SceneHandle, Error> AssetSystem::loadScene(const std::filesystem::path &source) {
      if (source.empty()) { return std::unexpected(Error::invalid_argument); }
      const auto path = normalizePath(source);
      Assimp::Importer importer{};
      constexpr auto flags = aiProcess_Triangulate | aiProcess_JoinIdenticalVertices |
                             aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace |
                             aiProcess_ImproveCacheLocality;
      const aiScene *scene = importer.ReadFile(path.string(), flags);
      if (scene == nullptr || scene->mRootNode == nullptr) { return std::unexpected(Error::asset_import_failed); }
      return importScene(catalog_, *scene, path);
   }

} // namespace vve::v4
