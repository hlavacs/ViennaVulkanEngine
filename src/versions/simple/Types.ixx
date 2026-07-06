export module VEEngine.Simple:Types;
import std;
export import VEEngine.Simple.Vector;
export import VEEngine.Simple.Error;
export import VEEngine.Simple.Math;
export import VEEngine.Types;
export import VEEngine.Simple.Handle;
export import VEEngine.Simple.Shaders;

/// @file
/// @brief simple implementation types.

export namespace vve::simple {

	using Scalar = math::Scalar;																			///< Short alias for the configured math scalar type.
	using Vec2	= math::Vec2;																				///< Short alias for the configured 2D vector type.
	using Vec3	= math::Vec3;																				///< Short alias for the configured 3D vector type.
	using Vec4	= math::Vec4;																				///< Short alias for the configured 4D vector type.
	using Quat	= math::Quat;																				///< Short alias for the configured quaternion type.
	using Mat4	= math::Mat4;																				///< Short alias for the configured 4x4 matrix type.


	using Entity	= vve::Entity;																	///< simple ECS entity handle contract.
	using SceneHandle	= vve::SceneHandle;															///< simple scene descriptor handle contract.
	using WindowHandle	= vve::WindowHandle;															///< simple runtime window handle contract.
	using NodeHandle	= vve::NodeHandle;															///< simple scene-node descriptor handle contract.
	using MeshHandle	= vve::MeshHandle;															///< simple mesh descriptor handle contract.
	using MaterialHandle = vve::MaterialHandle;														///< simple material descriptor handle contract.
	using TextureHandle	= vve::TextureHandle;														///< simple texture descriptor handle contract.
	using RenderSceneInstanceHandle = vve::RenderSceneInstanceHandle;							///< simple render-scene-instance handle contract.
	using LightHandle	= vve::LightHandle;															///< simple light descriptor handle contract.
	using CameraHandle	= vve::CameraHandle;															///< simple imported-camera descriptor handle contract.

	using ApplicationName = vve::ApplicationName;													///< simple app-name contract.
	using EngineConfig	= vve::EngineConfig;														///< simple compact engine config contract.
	using FrameContext	= vve::FrameContext;														///< simple frame context contract.
	using FrameStatus	= vve::FrameStatus;															///< simple frame status contract.
	using MaxFrames	= vve::MaxFrames;															///< simple frame-cap option contract.

	struct ResourceHandleTag {};																			///< simple-internal resource handle tag.
	struct RendererHandleTag {};																			///< simple-internal renderer descriptor handle tag.
	struct GuiWidgetHandleTag {};																			///< simple-internal GUI widget handle tag.

	using ResourceHandle	= TypedHandle<ResourceHandleTag>;										///< simple-internal resource handle.
	using RendererHandle	= TypedHandle<RendererHandleTag>;										///< simple-internal renderer descriptor handle.
	using GuiWidgetHandle = TypedHandle<GuiWidgetHandleTag>;										///< simple-internal GUI widget handle.

	using vve::Bounds;																						///< Public axis-aligned bounds contract.
	using vve::Camera;																						///< Public camera contract.
	using vve::CameraDescriptor;																			///< Public imported-camera descriptor contract.
	using vve::ClipPlanes;																					///< Public clip-plane contract.
	using vve::DeltaTime;																					///< Public frame delta contract.
	using vve::Direction;																					///< Public direction contract.
	using vve::FovY;																							///< Public vertical field-of-view contract.
	using vve::FrameCount;																					///< Public frame-count contract.
	using vve::IndexCount;																					///< Public index-count contract.
	using vve::LightDescriptor;																				///< Public imported-light descriptor contract.
	using vve::LightIntensity;																				///< Public light-intensity contract.
	using vve::LightKind;																						///< Public imported-light kind contract.
	using vve::LightRange;																					///< Public light-range contract.
	using vve::LinearColor;																					///< Public linear-color contract.
	using vve::ObjectName;																					///< Public object-name contract.
	using vve::PixelExtent;																					///< Public pixel-extent contract.
	using vve::Position;																						///< Public position contract.
	using vve::RendererId;																					///< Public renderer-id contract.
	using vve::Rotation;																						///< Public rotation contract.
	using vve::Scale;																							///< Public scale contract.
	using SceneInstantiationOptions = vve::SceneInstantiationOptions;								///< simple scene-instantiation option contract.
	using vve::SpotConeAngle;																				///< Public spotlight-cone contract.
	using vve::TextureChannelCount;																		///< Public texture-channel-count contract.
	using vve::Transform;																					///< Public transform contract.
	using vve::VertexCount;																					///< Public vertex-count contract.

} // namespace vve::simple
