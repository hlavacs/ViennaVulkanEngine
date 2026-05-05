module VEEngine.V4:AssetTypes;
import std;
import :Types;
import VEEngine.V4.Graph;

/// @file
/// @brief Private v4 asset descriptor and catalog types.

namespace vve::v4 {

   using Tree = BasicTree<NodeHandle>; ///< Internal scene-tree topology.

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
