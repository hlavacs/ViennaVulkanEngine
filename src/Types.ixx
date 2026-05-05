export module VEEngine:Types;
import std;
import VEEngine.V4;
import :Error;
import :Math;
import :Handle;

/**
 * @file
 * @brief Public type contract backed by the selected engine implementation.
 */
export namespace vve {

   template <typename T>
   using Vector = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::Vector<T>; ///< Facade dynamic array type.

   using Bounds              = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::Bounds;              ///< Facade bounds type.
   using Camera              = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::Camera;              ///< Facade camera type.
   using CameraDescriptor    = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::CameraDescriptor;    ///< Imported camera data.
   using CameraHandle        = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::CameraHandle;        ///< Camera descriptor handle.
   using ClipPlanes          = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::ClipPlanes;          ///< Facade clip planes.
   using DeltaTime           = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::DeltaTime;           ///< Facade delta time.
   using Direction           = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::Direction;           ///< Facade direction type.
   using Entity              = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::Entity;              ///< Facade ECS entity.
   using FovY                = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::FovY;                ///< Facade vertical FOV.
   using FrameCount          = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::FrameCount;          ///< Facade frame count.
   using IndexCount          = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::IndexCount;          ///< Imported index count.
   using LightDescriptor     = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::LightDescriptor;     ///< Imported light data.
   using LightHandle         = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::LightHandle;         ///< Light descriptor handle.
   using LightIntensity      = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::LightIntensity;      ///< Facade light intensity.
   using LightKind           = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::LightKind;           ///< Facade light kind.
   using LinearColor         = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::LinearColor;         ///< Facade linear color.
   using MaterialDescriptor  = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::MaterialDescriptor;  ///< Imported material data.
   using MaterialHandle      = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::MaterialHandle; ///< Material descriptor handle.
   using MeshDescriptor      = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::MeshDescriptor;      ///< Imported mesh data.
   using MeshHandle          = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::MeshHandle;          ///< Mesh descriptor handle.
   using MeshUse             = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::MeshUse;             ///< Node mesh reference.
   using NodeDescriptor      = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::NodeDescriptor;      ///< Imported node data.
   using NodeHandle          = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::NodeHandle;          ///< Node descriptor handle.
   using ObjectCatalog       = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::ObjectCatalog;       ///< Imported object catalog.
   using ObjectName          = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::ObjectName;          ///< Facade object name.
   using PixelExtent         = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::PixelExtent;         ///< Facade pixel extent.
   using Position            = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::Position;            ///< Facade position.
   using RendererId          = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::RendererId;          ///< Facade renderer id.
   using Rotation            = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::Rotation;            ///< Facade rotation.
   using Scale               = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::Scale;               ///< Facade scale.
   using SceneDescriptor     = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::SceneDescriptor;     ///< Imported scene data.
   using SceneHandle         = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::SceneHandle;         ///< Scene descriptor handle.
   using TextureBinding      = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::TextureBinding;      ///< Material texture binding.
   using TextureChannelCount = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::TextureChannelCount; ///< Texture channel count.
   using TextureDescriptor   = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::TextureDescriptor;   ///< Imported texture data.
   using TextureHandle       = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::TextureHandle;       ///< Texture descriptor handle.
   using TextureSemantic     = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::TextureSemantic;     ///< Texture slot semantic.
   using Transform           = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::Transform;           ///< Facade transform.
   using Tree                = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::Tree;                ///< Facade scene tree.
   using VertexCount         = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::VertexCount;         ///< Imported vertex count.
   using WindowHandle        = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::WindowHandle;        ///< Runtime window handle.

   template <typename TContainer, typename TValue> concept VectorLike =
      requires(TContainer container, TValue value) {
         { container.push_back(value) };
         { container.size() } -> std::convertible_to<std::size_t>;
         { container.begin() };
         { container.end() };
      }; ///< Contract for the public dynamic array alias.

   template <typename T> concept EntityLike =
      TypedHandleLike<T> && std::same_as<std::remove_cvref_t<T>, Entity>;
   template <typename T> concept SceneHandleLike =
      TypedHandleLike<T> && std::same_as<std::remove_cvref_t<T>, SceneHandle>;
   template <typename T> concept WindowHandleLike =
      TypedHandleLike<T> && std::same_as<std::remove_cvref_t<T>, WindowHandle>;
   template <typename T> concept NodeHandleLike =
      TypedHandleLike<T> && std::same_as<std::remove_cvref_t<T>, NodeHandle>;
   template <typename T> concept MeshHandleLike =
      TypedHandleLike<T> && std::same_as<std::remove_cvref_t<T>, MeshHandle>;
   template <typename T> concept MaterialHandleLike =
      TypedHandleLike<T> && std::same_as<std::remove_cvref_t<T>, MaterialHandle>;
   template <typename T> concept TextureHandleLike =
      TypedHandleLike<T> && std::same_as<std::remove_cvref_t<T>, TextureHandle>;
   template <typename T> concept LightHandleLike =
      TypedHandleLike<T> && std::same_as<std::remove_cvref_t<T>, LightHandle>;
   template <typename T> concept CameraHandleLike =
      TypedHandleLike<T> && std::same_as<std::remove_cvref_t<T>, CameraHandle>;

   template <typename T> concept PositionLike = requires(T value) {
      { value.value } -> std::same_as<Vec3 &>;
   }; ///< Contract for position wrappers.
   template <typename T> concept DirectionLike = requires(T value) {
      { value.value } -> std::same_as<Vec3 &>;
   }; ///< Contract for direction wrappers.
   template <typename T> concept ScaleLike = requires(T value) {
      { value.value } -> std::same_as<Vec3 &>;
   }; ///< Contract for scale wrappers.
   template <typename T> concept RotationLike = requires(T value) {
      { value.value } -> std::same_as<Quat &>;
   }; ///< Contract for rotation wrappers.
   template <typename T> concept LinearColorLike = requires(T value) {
      { value.value } -> std::same_as<Vec3 &>;
   }; ///< Contract for linear-color wrappers.
   template <typename T> concept LightIntensityLike = requires(T value) {
      { value.value } -> std::same_as<Scalar &>;
   }; ///< Contract for light-intensity wrappers.
   template <typename T> concept FovYLike = requires(T value) {
      { value.radians } -> std::same_as<Scalar &>;
   }; ///< Contract for vertical field-of-view wrappers.
   template <typename T> concept ClipPlanesLike = requires(T value) {
      { value.near_plane } -> std::same_as<Scalar &>;
      { value.far_plane } -> std::same_as<Scalar &>;
   }; ///< Contract for clip-plane wrappers.
   template <typename T> concept DeltaTimeLike = requires(T value) {
      { value.seconds } -> std::same_as<double &>;
   }; ///< Contract for frame-delta wrappers.
   template <typename T> concept PixelExtentLike = requires(T value) {
      { value.width } -> std::same_as<std::uint32_t &>;
      { value.height } -> std::same_as<std::uint32_t &>;
   }; ///< Contract for pixel-extent wrappers.
   template <typename T> concept ObjectNameLike = requires(T value) {
      { value.value } -> std::same_as<std::string &>;
   }; ///< Contract for object-name wrappers.
   template <typename T> concept RendererIdLike = requires(T value) {
      { value.value } -> std::same_as<std::string &>;
   }; ///< Contract for renderer-id wrappers.
   template <typename T> concept FrameCountLike = requires(T value) {
      { value.value } -> std::same_as<std::uint64_t &>;
   }; ///< Contract for frame-count wrappers.
   template <typename T> concept VertexCountLike = requires(T value) {
      { value.value } -> std::same_as<std::uint64_t &>;
   }; ///< Contract for vertex-count wrappers.
   template <typename T> concept IndexCountLike = requires(T value) {
      { value.value } -> std::same_as<std::uint64_t &>;
   }; ///< Contract for index-count wrappers.
   template <typename T> concept TextureChannelCountLike = requires(T value) {
      { value.value } -> std::same_as<std::uint32_t &>;
   }; ///< Contract for texture-channel wrappers.

   template <typename T> concept TransformLike = requires(T value) {
      { value.translation } -> std::same_as<Position &>;
      { value.rotation } -> std::same_as<Rotation &>;
      { value.scale } -> std::same_as<Scale &>;
   }; ///< Contract for transform structs.
   template <typename T> concept BoundsLike = requires(T value) {
      { value.minimum } -> std::same_as<Position &>;
      { value.maximum } -> std::same_as<Position &>;
      { value.valid } -> std::same_as<bool &>;
   }; ///< Contract for axis-aligned bounds structs.
   template <typename T> concept CameraLike =
      requires(std::remove_cvref_t<T> camera, Position position, Direction up, FovY fov, ClipPlanes clip) {
         { camera.position } -> std::same_as<Position &>;
         { camera.forward } -> std::same_as<Direction &>;
         { camera.view_transform } -> std::same_as<Mat4 &>;
         { camera.fov_y } -> std::same_as<FovY &>;
         { camera.clip } -> std::same_as<ClipPlanes &>;
         { std::remove_cvref_t<T>::lookAt(position, position, up, fov, clip) } ->
            std::same_as<std::remove_cvref_t<T>>;
      }; ///< Contract for camera structs.

   template <typename T> concept TextureBindingLike = requires(T value) {
      { value.texture } -> std::same_as<TextureHandle &>;
      { value.semantic } -> std::same_as<TextureSemantic &>;
      { value.uv_set } -> std::same_as<std::uint32_t &>;
   }; ///< Contract for material texture bindings.
   template <typename T> concept MeshUseLike = requires(T value) {
      { value.mesh } -> std::same_as<MeshHandle &>;
      { value.material } -> std::same_as<MaterialHandle &>;
   }; ///< Contract for node mesh references.
   template <typename T> concept NodeDescriptorLike = requires(T value) {
      typename T::HandleType;
      { value.handle } -> std::same_as<NodeHandle &>;
      { value.name } -> std::same_as<ObjectName &>;
      { value.transform } -> std::same_as<Transform &>;
      { value.meshes } -> std::same_as<Vector<MeshUse> &>;
   }; ///< Contract for imported node descriptors.
   template <typename T> concept MeshDescriptorLike = requires(T value) {
      typename T::HandleType;
      { value.handle } -> std::same_as<MeshHandle &>;
      { value.name } -> std::same_as<ObjectName &>;
      { value.vertex_count } -> std::same_as<VertexCount &>;
      { value.index_count } -> std::same_as<IndexCount &>;
      { value.material } -> std::same_as<MaterialHandle &>;
      { value.bounds } -> std::same_as<Bounds &>;
   }; ///< Contract for imported mesh descriptors.
   template <typename T> concept MaterialDescriptorLike = requires(T value) {
      typename T::HandleType;
      { value.handle } -> std::same_as<MaterialHandle &>;
      { value.name } -> std::same_as<ObjectName &>;
      { value.textures } -> std::same_as<Vector<TextureBinding> &>;
   }; ///< Contract for imported material descriptors.
   template <typename T> concept TextureDescriptorLike = requires(T value) {
      typename T::HandleType;
      { value.handle } -> std::same_as<TextureHandle &>;
      { value.name } -> std::same_as<ObjectName &>;
      { value.source } -> std::same_as<std::filesystem::path &>;
      { value.extent } -> std::same_as<PixelExtent &>;
      { value.channels } -> std::same_as<TextureChannelCount &>;
   }; ///< Contract for imported texture descriptors.
   template <typename T> concept LightDescriptorLike = requires(T value) {
      typename T::HandleType;
      { value.handle } -> std::same_as<LightHandle &>;
      { value.name } -> std::same_as<ObjectName &>;
      { value.kind } -> std::same_as<LightKind &>;
      { value.position } -> std::same_as<Position &>;
      { value.direction } -> std::same_as<Direction &>;
      { value.color } -> std::same_as<LinearColor &>;
      { value.intensity } -> std::same_as<LightIntensity &>;
   }; ///< Contract for imported light descriptors.
   template <typename T> concept CameraDescriptorLike = requires(T value) {
      typename T::HandleType;
      { value.handle } -> std::same_as<CameraHandle &>;
      { value.name } -> std::same_as<ObjectName &>;
      { value.position } -> std::same_as<Position &>;
      { value.forward } -> std::same_as<Direction &>;
      { value.fov_y } -> std::same_as<FovY &>;
      { value.clip } -> std::same_as<ClipPlanes &>;
   }; ///< Contract for imported camera descriptors.
   template <typename T> concept TreeLike = requires(T tree, NodeHandle parent, NodeHandle child) {
      { tree.root } -> std::same_as<NodeHandle &>;
      tree.addChild(parent, child);
      tree.removeNode(child);
      { tree.childRange(parent) };
      { tree.parentOf(child) } -> std::same_as<std::optional<NodeHandle>>;
   }; ///< Contract for scene tree topology.
   template <typename T> concept SceneDescriptorLike = requires(T value) {
      typename T::HandleType;
      { value.handle } -> std::same_as<SceneHandle &>;
      { value.name } -> std::same_as<ObjectName &>;
      { value.tree } -> std::same_as<Tree &>;
      { value.nodes } -> std::same_as<Vector<NodeHandle> &>;
      { value.meshes } -> std::same_as<Vector<MeshHandle> &>;
      { value.materials } -> std::same_as<Vector<MaterialHandle> &>;
      { value.textures } -> std::same_as<Vector<TextureHandle> &>;
      { value.lights } -> std::same_as<Vector<LightHandle> &>;
      { value.cameras } -> std::same_as<Vector<CameraHandle> &>;
   }; ///< Contract for imported scene descriptors.
   template <typename T> concept ObjectCatalogLike = requires(T catalog) {
      { catalog.scenes.add(SceneDescriptor{}) } -> std::same_as<std::expected<void, Error>>;
      { catalog.nodes.add(NodeDescriptor{}) } -> std::same_as<std::expected<void, Error>>;
      { catalog.meshes.find(MeshHandle{}) } -> std::same_as<MeshDescriptor *>;
      { catalog.materials.find(MaterialHandle{}) } -> std::same_as<MaterialDescriptor *>;
      { catalog.textures.remove(TextureHandle{}) } -> std::same_as<std::expected<void, Error>>;
      { catalog.lights.contains(LightHandle{}) } -> std::same_as<bool>;
      { catalog.cameras.size() } -> std::convertible_to<std::size_t>;
   }; ///< Contract for the public imported-object catalog.

   static_assert(BoundsLike<Bounds>);
   static_assert(CameraDescriptorLike<CameraDescriptor>);
   static_assert(CameraHandleLike<CameraHandle>);
   static_assert(CameraLike<Camera>);
   static_assert(ClipPlanesLike<ClipPlanes>);
   static_assert(CounterHandleFactoryLike<MeshHandle>);
   static_assert(DeltaTimeLike<DeltaTime>);
   static_assert(DirectionLike<Direction>);
   static_assert(EntityLike<Entity>);
   static_assert(FovYLike<FovY>);
   static_assert(FrameCountLike<FrameCount>);
   static_assert(IndexCountLike<IndexCount>);
   static_assert(LightDescriptorLike<LightDescriptor>);
   static_assert(LightHandleLike<LightHandle>);
   static_assert(LightIntensityLike<LightIntensity>);
   static_assert(LinearColorLike<LinearColor>);
   static_assert(MaterialDescriptorLike<MaterialDescriptor>);
   static_assert(MaterialHandleLike<MaterialHandle>);
   static_assert(MeshDescriptorLike<MeshDescriptor>);
   static_assert(MeshHandleLike<MeshHandle>);
   static_assert(MeshUseLike<MeshUse>);
   static_assert(NodeDescriptorLike<NodeDescriptor>);
   static_assert(NodeHandleLike<NodeHandle>);
   static_assert(ObjectCatalogLike<ObjectCatalog>);
   static_assert(ObjectNameLike<ObjectName>);
   static_assert(PixelExtentLike<PixelExtent>);
   static_assert(PositionLike<Position>);
   static_assert(RendererIdLike<RendererId>);
   static_assert(RotationLike<Rotation>);
   static_assert(ScaleLike<Scale>);
   static_assert(SceneDescriptorLike<SceneDescriptor>);
   static_assert(SceneHandleLike<SceneHandle>);
   static_assert(SlotMapHandleFactoryLike<MeshHandle>);
   static_assert(TestHandleFactoryLike<MeshHandle>);
   static_assert(TextureBindingLike<TextureBinding>);
   static_assert(TextureChannelCountLike<TextureChannelCount>);
   static_assert(TextureDescriptorLike<TextureDescriptor>);
   static_assert(TextureHandleLike<TextureHandle>);
   static_assert(TransformLike<Transform>);
   static_assert(TreeLike<Tree>);
   static_assert(TypedHandleLike<Entity>);
   static_assert(TypedHandleLike<MeshHandle>);
   static_assert(VectorLike<Vector<int>, int>);
   static_assert(VertexCountLike<VertexCount>);
   static_assert(WindowHandleLike<WindowHandle>);

} // namespace vve
