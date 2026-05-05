export module VEEngine:Types;
import std;
import VEEngine.V4;
import :Error;
import :Math;
import :Handle;
import :Vector;

/**
 * @file
 * @brief Public type contract backed by the selected engine implementation.
 */
export namespace vve {

   using Bounds              = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::Bounds;              ///< Facade bounds type.
   using Camera              = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::Camera;              ///< Facade camera type.
   using CameraDescriptor    = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::CameraDescriptor;    ///< Imported camera data.
   using CameraHandle        = TypedHandle<typename VVE_ENGINE_IMPLEMENTATION_NAMESPACE::CameraHandle::tag_type>; ///< Camera descriptor handle.
   using ClipPlanes          = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::ClipPlanes;          ///< Facade clip planes.
   using DeltaTime           = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::DeltaTime;           ///< Facade delta time.
   using Direction           = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::Direction;           ///< Facade direction type.
   using Entity              = TypedHandle<typename VVE_ENGINE_IMPLEMENTATION_NAMESPACE::Entity::tag_type>; ///< Facade ECS entity.
   using FovY                = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::FovY;                ///< Facade vertical FOV.
   using FrameCount          = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::FrameCount;          ///< Facade frame count.
   using IndexCount          = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::IndexCount;          ///< Imported index count.
   using LightDescriptor     = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::LightDescriptor;     ///< Imported light data.
   using LightHandle         = TypedHandle<typename VVE_ENGINE_IMPLEMENTATION_NAMESPACE::LightHandle::tag_type>; ///< Light descriptor handle.
   using LightIntensity      = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::LightIntensity;      ///< Facade light intensity.
   using LightKind           = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::LightKind;           ///< Facade light kind.
   using LinearColor         = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::LinearColor;         ///< Facade linear color.
   using MaterialDescriptor  = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::MaterialDescriptor;  ///< Imported material data.
   using MaterialHandle      = TypedHandle<typename VVE_ENGINE_IMPLEMENTATION_NAMESPACE::MaterialHandle::tag_type>; ///< Material descriptor handle.
   using MeshDescriptor      = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::MeshDescriptor;      ///< Imported mesh data.
   using MeshHandle          = TypedHandle<typename VVE_ENGINE_IMPLEMENTATION_NAMESPACE::MeshHandle::tag_type>; ///< Mesh descriptor handle.
   using MeshUse             = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::MeshUse;             ///< Node mesh reference.
   using NodeDescriptor      = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::NodeDescriptor;      ///< Imported node data.
   using NodeHandle          = TypedHandle<typename VVE_ENGINE_IMPLEMENTATION_NAMESPACE::NodeHandle::tag_type>; ///< Node descriptor handle.
   using ObjectCatalog       = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::ObjectCatalog;       ///< Imported object catalog.
   using ObjectName          = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::ObjectName;          ///< Facade object name.
   using PixelExtent         = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::PixelExtent;         ///< Facade pixel extent.
   using Position            = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::Position;            ///< Facade position.
   using RendererId          = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::RendererId;          ///< Facade renderer id.
   using Rotation            = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::Rotation;            ///< Facade rotation.
   using Scale               = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::Scale;               ///< Facade scale.
   using SceneDescriptor     = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::SceneDescriptor;     ///< Imported scene data.
   using SceneHandle         = TypedHandle<typename VVE_ENGINE_IMPLEMENTATION_NAMESPACE::SceneHandle::tag_type>; ///< Scene descriptor handle.
   using TextureBinding      = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::TextureBinding;      ///< Material texture binding.
   using TextureChannelCount = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::TextureChannelCount; ///< Texture channel count.
   using TextureDescriptor   = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::TextureDescriptor;   ///< Imported texture data.
   using TextureHandle       = TypedHandle<typename VVE_ENGINE_IMPLEMENTATION_NAMESPACE::TextureHandle::tag_type>; ///< Texture descriptor handle.
   using TextureSemantic     = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::TextureSemantic;     ///< Texture slot semantic.
   using Transform           = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::Transform;           ///< Facade transform.
   using Tree                = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::Tree;                ///< Facade scene tree.
   using VertexCount         = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::VertexCount;         ///< Imported vertex count.
   using WindowHandle        = TypedHandle<typename VVE_ENGINE_IMPLEMENTATION_NAMESPACE::WindowHandle::tag_type>; ///< Runtime window handle.


} // namespace vve
