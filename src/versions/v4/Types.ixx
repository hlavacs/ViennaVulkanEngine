module;

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/ext/quaternion_double.hpp>
#include <glm/ext/quaternion_float.hpp>
#include <glm/ext/vector_double2.hpp>
#include <glm/ext/vector_double3.hpp>
#include <glm/ext/vector_double4.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>

export module VEEngine.V4:Types;
import std;
export import :ECS;

/// @file
/// @brief Strong math wrappers and handle-addressable descriptor types for v4.

export namespace vve::v4 {

#if defined(VVE_MATH_USE_DOUBLE)
   using Scalar = double;     ///< Engine scalar type when double precision is enabled.
   using Vec2   = glm::dvec2; ///< GLM 2D vector using the selected scalar precision.
   using Vec3   = glm::dvec3; ///< GLM 3D vector using the selected scalar precision.
   using Vec4   = glm::dvec4; ///< GLM 4D vector using the selected scalar precision.
   using Quat   = glm::dquat; ///< GLM quaternion using the selected scalar precision.
#else
   using Scalar = float;     ///< Engine scalar type used by the default v4 build.
   using Vec2   = glm::vec2; ///< GLM 2D vector using the selected scalar precision.
   using Vec3   = glm::vec3; ///< GLM 3D vector using the selected scalar precision.
   using Vec4   = glm::vec4; ///< GLM 4D vector using the selected scalar precision.
   using Quat   = glm::quat; ///< GLM quaternion using the selected scalar precision.
#endif

   /// @brief Returns the additive identity for the selected scalar type.
   [[nodiscard]] inline constexpr Scalar zero() noexcept { return static_cast<Scalar>(0); }
   /// @brief Returns the multiplicative identity for the selected scalar type.
   [[nodiscard]] inline constexpr Scalar one() noexcept { return static_cast<Scalar>(1); }
   /// @brief Returns the zero vector.
   [[nodiscard]] inline Vec3 zeroVec3() noexcept { return Vec3(zero(), zero(), zero()); }
   /// @brief Returns a vector with all coordinates set to one.
   [[nodiscard]] inline Vec3 oneVec3() noexcept { return Vec3(one(), one(), one()); }
   /// @brief Returns the identity rotation.
   [[nodiscard]] inline Quat identityQuat() noexcept { return Quat(one(), zero(), zero(), zero()); }

   /// @brief Strong wrapper for world or local position values.
   struct Position {
      Vec3 value{zeroVec3()}; ///< Wrapped GLM coordinate.
   };

   /// @brief Strong wrapper for orientation vectors that should be interpreted as directions.
   struct Direction {
      Vec3 value{0.0F, 0.0F, -1.0F}; ///< Wrapped GLM direction; defaults to forward.
   };

   /// @brief Strong wrapper for non-uniform scale factors.
   struct Scale {
      Vec3 value{oneVec3()}; ///< Wrapped GLM scale vector.
   };

   /// @brief Strong wrapper for quaternion rotations.
   struct Rotation {
      Quat value{identityQuat()}; ///< Wrapped GLM quaternion.
   };

   /// @brief Transform kept as explicit strong fields for textbook readability.
   struct Transform {
      Position position{}; ///< Local position.
      Rotation rotation{}; ///< Local orientation.
      Scale scale{};       ///< Local scale.
   };

   /// @brief Axis-aligned bounds in descriptor space.
   struct Bounds {
      Position minimum{}; ///< Minimum corner.
      Position maximum{}; ///< Maximum corner.
      bool valid{false};  ///< False until at least one point has been included.
   };

   /// @brief Material texture slot meaning.
   enum class TextureSemantic {
      unknown,    ///< Unclassified texture use.
      base_color, ///< Color/albedo texture.
      normal,     ///< Tangent-space normal texture.
      roughness,  ///< Roughness texture.
      metallic,   ///< Metallic texture.
      emissive,   ///< Emissive texture.
      occlusion   ///< Ambient-occlusion texture.
   };

   /// @brief High-level light shape.
   enum class LightKind {
      unknown,     ///< Unclassified light.
      directional, ///< Direction-only light such as the sun.
      point,       ///< Point light with position.
      spot         ///< Spot light with position and direction.
   };

   /// @brief A material reference to one texture descriptor.
   struct TextureBinding {
      Handle texture{};                                    ///< Referenced TextureDescriptor handle.
      TextureSemantic semantic{TextureSemantic::unknown};  ///< Intended material slot.
      std::uint32_t uv_set{0};                             ///< UV channel used by the texture.
   };

   /// @brief A scene node reference to renderable geometry and material.
   struct MeshUse {
      Handle mesh{};     ///< Referenced MeshDescriptor handle.
      Handle material{}; ///< Referenced MaterialDescriptor handle.
   };

   /// @brief Tree topology: one root plus parent-to-child handle edges.
   struct Tree {
      Handle root{};                           ///< Root node handle.
      std::multimap<Handle, Handle> children{}; ///< Parent node handle mapped to child node handles.

      /// @brief Adds one parent-to-child tree edge.
      void addChild(Handle parent, Handle child) { children.emplace(parent, child); }
      /// @brief Returns all children for a parent handle.
      [[nodiscard]] auto childRange(Handle parent) const { return children.equal_range(parent); }
   };

   /// @brief Generic directed graph topology stored as parent-to-child handle edges.
   struct Graph {
      std::multimap<Handle, Handle> edges{}; ///< Source node handle mapped to destination node handles.

      /// @brief Adds one directed edge.
      void addEdge(Handle from, Handle to) { edges.emplace(from, to); }
      /// @brief Returns all outgoing edges for a node handle.
      [[nodiscard]] auto childRange(Handle node) const { return edges.equal_range(node); }
   };

   /// @brief Scene graph node descriptor stored by handle in ObjectCatalog.
   struct NodeDescriptor {
      Handle handle{};               ///< Stable 64-bit node handle.
      std::string name{};            ///< Human-readable node name.
      Transform transform{};         ///< Local transform.
      Vector<MeshUse> meshes{};      ///< Mesh/material pairs attached to this node.
   };

   /// @brief Mesh geometry descriptor; actual vertex buffers are added later.
   struct MeshDescriptor {
      Handle handle{};              ///< Stable 64-bit mesh handle.
      std::string name{};           ///< Human-readable mesh name.
      std::uint64_t vertex_count{0}; ///< Number of vertices in source geometry.
      std::uint64_t index_count{0};  ///< Number of indices in source geometry.
      Handle material{};            ///< Default material handle.
      Bounds bounds{};              ///< Object-space bounds.
   };

   /// @brief Material descriptor referencing textures by handle.
   struct MaterialDescriptor {
      Handle handle{};                  ///< Stable 64-bit material handle.
      std::string name{};               ///< Human-readable material name.
      Vector<TextureBinding> textures{}; ///< Texture slots used by this material.
   };

   /// @brief Texture descriptor; pixel storage and GPU upload are future steps.
   struct TextureDescriptor {
      Handle handle{};             ///< Stable 64-bit texture handle.
      std::string name{};          ///< Human-readable texture name.
      std::filesystem::path source{}; ///< Source file path or logical asset path.
      std::uint32_t width{0};      ///< Source width in pixels.
      std::uint32_t height{0};     ///< Source height in pixels.
      std::uint32_t channels{0};   ///< Source channel count.
   };

   /// @brief Light descriptor used by renderers and scene systems.
   struct LightDescriptor {
      Handle handle{};                    ///< Stable 64-bit light handle.
      std::string name{};                 ///< Human-readable light name.
      LightKind kind{LightKind::unknown}; ///< Light shape.
      Position position{};                ///< Light position for point/spot lights.
      Direction direction{};              ///< Light direction for directional/spot lights.
      Vec3 color{1.0F, 1.0F, 1.0F};       ///< Linear light color.
      Scalar intensity{1.0F};             ///< Relative light intensity.
   };

   /// @brief Camera descriptor used to create runtime cameras.
   struct CameraDescriptor {
      Handle handle{};                    ///< Stable 64-bit camera handle.
      std::string name{};                 ///< Human-readable camera name.
      Position position{};                ///< Camera position.
      Direction forward{};                ///< Camera forward direction.
      Scalar fov_y_radians{1.0471976F};   ///< Vertical field of view.
      Scalar near_plane{0.1F};            ///< Near clipping plane.
      Scalar far_plane{1000.0F};          ///< Far clipping plane.
   };

   /// @brief Scene descriptor stores only handles to objects kept in descriptor maps.
   struct SceneDescriptor {
      Handle handle{};           ///< Stable 64-bit scene handle.
      std::string name{};        ///< Human-readable scene name.
      Tree tree{};               ///< Scene hierarchy; nodes do not store child vectors.
      Vector<Handle> nodes{};    ///< All node handles in the scene.
      Vector<Handle> meshes{};   ///< Mesh handles used by the scene.
      Vector<Handle> materials{}; ///< Material handles used by the scene.
      Vector<Handle> textures{}; ///< Texture handles used by the scene.
      Vector<Handle> lights{};   ///< Light handles used by the scene.
      Vector<Handle> cameras{};  ///< Camera handles used by the scene.
   };

   /// @brief Simple descriptor table keyed by Handle.
   template <typename TDescriptor> class DescriptorMap {
   public:
      /// @brief Inserts a descriptor; descriptors must expose a valid `handle` member.
      [[nodiscard]] std::expected<void, Error> add(TDescriptor descriptor) {
         if (!descriptor.handle.valid()) {
            return std::unexpected(Error::invalid_handle);
         }
         const auto [_, inserted] = descriptors_.emplace(descriptor.handle, std::move(descriptor));
         if (!inserted) {
            return std::unexpected(Error::duplicate_object);
         }
         return {};
      }

      /// @brief Finds a descriptor by handle, or returns null.
      [[nodiscard]] const TDescriptor *find(Handle handle) const {
         const auto it = descriptors_.find(handle);
         return it == descriptors_.end() ? nullptr : std::addressof(it->second);
      }

      /// @brief Finds a mutable descriptor by handle, or returns null.
      [[nodiscard]] TDescriptor *find(Handle handle) {
         const auto it = descriptors_.find(handle);
         return it == descriptors_.end() ? nullptr : std::addressof(it->second);
      }

      /// @brief Returns true when the map contains the handle.
      [[nodiscard]] bool contains(Handle handle) const { return descriptors_.contains(handle); }
      /// @brief Returns descriptor count.
      [[nodiscard]] std::size_t size() const { return descriptors_.size(); }
      /// @brief Exposes read-only descriptor storage for tests and iteration.
      [[nodiscard]] const std::map<Handle, TDescriptor> &all() const { return descriptors_; }

   private:
      std::map<Handle, TDescriptor> descriptors_{}; ///< Ordered descriptor storage.
   };

   /// @brief Central imported-object catalog; every loaded object is found by 64-bit handle.
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
