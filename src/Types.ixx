export module VEEngine.Types;
import std;
export import VEEngine.Error;
export import VEEngine.Handle;
export import VEEngine.Math;
export import VEEngine.Vector;

/**
 * @file
 * @brief Public data and descriptor contract declared by the facade layer.
 */
export namespace vve {

   struct EntityTag;   ///< Entity handle category.
   struct SceneTag;    ///< Scene handle category.
   struct WindowTag;   ///< Window handle category.
   struct NodeTag;     ///< Scene node handle category.
   struct MeshTag;     ///< Mesh handle category.
   struct MaterialTag; ///< Material handle category.
   struct TextureTag;  ///< Texture handle category.
   struct LightTag;    ///< Light handle category.
   struct CameraTag;   ///< Camera handle category.

   using Entity         = TypedHandle<EntityTag>;   ///< Facade ECS entity.
   using SceneHandle    = TypedHandle<SceneTag>;    ///< Scene descriptor handle.
   using WindowHandle   = TypedHandle<WindowTag>;   ///< Runtime window handle.
   using NodeHandle     = TypedHandle<NodeTag>;     ///< Node descriptor handle.
   using MeshHandle     = TypedHandle<MeshTag>;     ///< Mesh descriptor handle.
   using MaterialHandle = TypedHandle<MaterialTag>; ///< Material descriptor handle.
   using TextureHandle  = TypedHandle<TextureTag>;  ///< Texture descriptor handle.
   using LightHandle    = TypedHandle<LightTag>;    ///< Light descriptor handle.
   using CameraHandle   = TypedHandle<CameraTag>;   ///< Camera descriptor handle.

   /// @brief Strong wrapper for world or local position values.
   struct Position {
      Vec3 value{zeroVec3()}; ///< Wrapped coordinate.
   };

   /// @brief Strong wrapper for vectors that should be interpreted as directions.
   struct Direction {
      Vec3 value{Vec3(zero(), zero(), -one())}; ///< Wrapped direction.
   };

   /// @brief Strong wrapper for non-uniform scale factors.
   struct Scale {
      Vec3 value{oneVec3()}; ///< Wrapped scale vector.
   };

   /// @brief Strong wrapper for quaternion rotations.
   struct Rotation {
      Quat value{identityQuat()}; ///< Wrapped orientation.
   };

   /// @brief Strong wrapper for linear RGB color values.
   struct LinearColor {
      Vec3 value{oneVec3()}; ///< Wrapped linear RGB color.
   };

   /// @brief Strong wrapper for relative light intensity.
   struct LightIntensity {
      Scalar value{one()}; ///< Wrapped non-negative intensity scale.
   };

   /// @brief Strong wrapper for vertical field-of-view angles.
   struct FovY {
      Scalar radians{static_cast<Scalar>(1.0471975511965976)}; ///< Wrapped vertical FOV in radians.
   };

   /// @brief Strong wrapper for near and far clipping planes.
   struct ClipPlanes {
      Scalar near_plane{static_cast<Scalar>(0.1)};     ///< Near clip distance.
      Scalar far_plane{static_cast<Scalar>(10000.0)}; ///< Far clip distance.
   };

   /// @brief Strong wrapper for frame delta time.
   struct DeltaTime {
      double seconds{1.0 / 60.0}; ///< Elapsed seconds.
   };

   /// @brief Strong wrapper for pixel dimensions.
   struct PixelExtent {
      std::uint32_t width{0};  ///< Width in pixels.
      std::uint32_t height{0}; ///< Height in pixels.
   };

   /// @brief Strong wrapper for human-readable object names.
   struct ObjectName {
      std::string value{}; ///< Wrapped display or diagnostic name.
   };

   /// @brief Strong wrapper for renderer selection identifiers.
   struct RendererId {
      std::string value{}; ///< Wrapped renderer identifier.
   };

   /// @brief Strong wrapper for frame counts and frame indices.
   struct FrameCount {
      std::uint64_t value{0}; ///< Wrapped frame count.
   };

   /// @brief Strong wrapper for source vertex counts.
   struct VertexCount {
      std::uint64_t value{0}; ///< Wrapped vertex count.
   };

   /// @brief Strong wrapper for source index counts.
   struct IndexCount {
      std::uint64_t value{0}; ///< Wrapped index count.
   };

   /// @brief Strong wrapper for texture channel counts.
   struct TextureChannelCount {
      std::uint32_t value{0}; ///< Wrapped channel count.
   };

   /// @brief Standard transform component shared by all active engine layers.
   struct Transform {
      Position translation{}; ///< Local or world-space translation.
      Rotation rotation{};    ///< Local or world-space orientation.
      Scale scale{};          ///< Local or world-space non-uniform scale.
   };

   /// @brief Axis-aligned bounds described by minimum and maximum positions.
   struct Bounds {
      Position minimum{}; ///< Minimum corner.
      Position maximum{}; ///< Maximum corner.
      bool valid{false};  ///< False until at least one point has been included.
   };

   /// @brief Material texture slot meaning for imported material descriptors.
   enum class TextureSemantic {
      unknown,    ///< Unclassified texture use.
      base_color, ///< Color/albedo texture.
      normal,     ///< Tangent-space normal texture.
      roughness,  ///< Roughness texture.
      metallic,   ///< Metallic texture.
      emissive,   ///< Emissive texture.
      occlusion   ///< Ambient-occlusion texture.
   };

   /// @brief High-level light shape visible to apps creating or inspecting lights.
   enum class LightKind {
      unknown,     ///< Unclassified light.
      directional, ///< Direction-only light such as the sun.
      point,       ///< Point light with position.
      spot         ///< Spot light with position and direction.
   };

   /// @brief Public camera description used by game code and renderers.
   struct Camera {
      Position position{.value = Vec3(zero(), static_cast<Scalar>(1.5), static_cast<Scalar>(6.0))};
      Direction forward{.value = Vec3(zero(), zero(), -one())}; ///< View direction.
      Mat4 view_transform{math::translate(identityMat4(),
                                          Vec3(zero(), static_cast<Scalar>(-1.5), static_cast<Scalar>(-6.0)))};
      FovY fov_y{};      ///< Vertical field of view.
      ClipPlanes clip{}; ///< Near/far clip planes.

      [[nodiscard]] static Camera lookAt(Position position, Position target,
                                         Direction up = Direction{.value = Vec3(zero(), one(), zero())},
                                         FovY fov_y = {}, ClipPlanes clip = {}) {
         Camera camera{};
         camera.position = position;
         camera.forward = Direction{.value = math::subtract(target.value, position.value)};
         camera.view_transform = math::lookAt(position.value, target.value, up.value);
         camera.fov_y = fov_y;
         camera.clip = clip;
         return camera;
      }
   };

   /// @brief A material reference to one texture descriptor.
   struct TextureBinding {
      TextureHandle texture{};                            ///< Referenced texture handle.
      TextureSemantic semantic{TextureSemantic::unknown}; ///< Intended material slot.
      std::uint32_t uv_set{0};                            ///< UV channel used by the texture.
   };

   /// @brief A scene node reference to renderable geometry and material.
   struct MeshUse {
      MeshHandle mesh{};         ///< Referenced mesh handle.
      MaterialHandle material{}; ///< Referenced material handle.
   };

   /// @brief Scene graph node descriptor stored by handle in the object catalog.
   struct NodeDescriptor {
      using HandleType = NodeHandle; ///< Descriptor handle type.
      NodeHandle handle{};           ///< Stable node handle.
      ObjectName name{};             ///< Human-readable node name.
      Transform transform{};         ///< Local transform.
      Vector<MeshUse> meshes{};      ///< Mesh/material pairs attached to this node.
   };

   /// @brief Mesh geometry descriptor; actual vertex buffers are implementation work.
   struct MeshDescriptor {
      using HandleType = MeshHandle; ///< Descriptor handle type.
      MeshHandle handle{};           ///< Stable mesh handle.
      ObjectName name{};             ///< Human-readable mesh name.
      VertexCount vertex_count{};    ///< Number of vertices in source geometry.
      IndexCount index_count{};      ///< Number of indices in source geometry.
      MaterialHandle material{};     ///< Default material handle.
      Bounds bounds{};               ///< Object-space bounds.
   };

   /// @brief Material descriptor referencing textures by handle.
   struct MaterialDescriptor {
      using HandleType = MaterialHandle; ///< Descriptor handle type.
      MaterialHandle handle{};           ///< Stable material handle.
      ObjectName name{};                 ///< Human-readable material name.
      Vector<TextureBinding> textures{}; ///< Texture slots used by this material.
   };

   /// @brief Texture descriptor; pixel storage and GPU upload are implementation work.
   struct TextureDescriptor {
      using HandleType = TextureHandle;    ///< Descriptor handle type.
      TextureHandle handle{};              ///< Stable texture handle.
      ObjectName name{};                   ///< Human-readable texture name.
      std::filesystem::path source{};      ///< Source file path or logical asset path.
      PixelExtent extent{};                ///< Source dimensions in pixels.
      TextureChannelCount channels{};      ///< Source channel count.
   };

   /// @brief Imported-light descriptor.
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

   /// @brief Imported-camera descriptor.
   struct CameraDescriptor {
      using HandleType = CameraHandle; ///< Descriptor handle type.
      CameraHandle handle{};           ///< Stable camera handle.
      ObjectName name{};               ///< Human-readable camera name.
      Position position{};             ///< Camera position.
      Direction forward{};             ///< Camera forward direction.
      FovY fov_y{};                    ///< Vertical field of view.
      ClipPlanes clip{};               ///< Near and far clipping planes.
   };

} // namespace vve
