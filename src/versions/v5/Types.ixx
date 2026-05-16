export module VEEngine.V5:Types;
import std;
export import VEEngine.V5.Vector;
export import VEEngine.V5.Error;
export import VEEngine.V5.Math;
export import VEEngine.Types;
export import VEEngine.V5.Handle;

/// @file
/// @brief v5 implementation types.

export namespace vve::v5 {

   using Scalar = math::Scalar; ///< Short alias for the configured math scalar type.
   using Vec2   = math::Vec2;   ///< Short alias for the configured 2D vector type.
   using Vec3   = math::Vec3;   ///< Short alias for the configured 3D vector type.
   using Vec4   = math::Vec4;   ///< Short alias for the configured 4D vector type.
   using Quat   = math::Quat;   ///< Short alias for the configured quaternion type.
   using Mat4   = math::Mat4;   ///< Short alias for the configured 4x4 matrix type.

   [[nodiscard]] inline constexpr Scalar zero() noexcept { return math::zero(); } ///< Scalar zero.
   [[nodiscard]] inline constexpr Scalar one() noexcept { return math::one(); } ///< Scalar one.
   [[nodiscard]] inline Vec3 zeroVec3() noexcept { return math::zeroVec3(); } ///< Zero 3D vector.
   [[nodiscard]] inline Vec3 oneVec3() noexcept { return math::oneVec3(); } ///< One-filled 3D vector.
   [[nodiscard]] inline Quat identityQuat() noexcept { return math::identityQuat(); } ///< Identity rotation.
   [[nodiscard]] inline Mat4 identityMat4() noexcept { return math::identityMat4(); } ///< Identity matrix.

   using Entity         = vve::Entity;         ///< v5 ECS entity handle contract.
   using SceneHandle    = vve::SceneHandle;    ///< v5 scene descriptor handle contract.
   using WindowHandle   = vve::WindowHandle;   ///< v5 runtime window handle contract.
   using NodeHandle     = vve::NodeHandle;     ///< v5 scene-node descriptor handle contract.
   using MeshHandle     = vve::MeshHandle;     ///< v5 mesh descriptor handle contract.
   using MaterialHandle = vve::MaterialHandle; ///< v5 material descriptor handle contract.
   using TextureHandle  = vve::TextureHandle;  ///< v5 texture descriptor handle contract.
   using LightHandle    = vve::LightHandle;    ///< v5 light descriptor handle contract.
   using CameraHandle   = vve::CameraHandle;   ///< v5 imported-camera descriptor handle contract.

   using ApplicationName = vve::ApplicationName; ///< v5 app-name contract.
   using EngineConfig    = vve::EngineConfig;    ///< v5 compact engine config contract.
   using FrameContext    = vve::FrameContext;    ///< v5 frame context contract.
   using FrameStatus     = vve::FrameStatus;     ///< v5 frame status contract.
   using MaxFrames       = vve::MaxFrames;       ///< v5 frame-cap option contract.

   struct ResourceHandleTag {};  ///< v5-internal resource handle tag.
   struct ShaderHandleTag {};    ///< v5-internal shader descriptor handle tag.
   struct RendererHandleTag {};  ///< v5-internal renderer descriptor handle tag.
   struct GuiWidgetHandleTag {}; ///< v5-internal GUI widget handle tag.

   using ResourceHandle  = TypedHandle<ResourceHandleTag>;  ///< v5-internal resource handle.
   using ShaderHandle    = TypedHandle<ShaderHandleTag>;    ///< v5-internal shader descriptor handle.
   using RendererHandle  = TypedHandle<RendererHandleTag>;  ///< v5-internal renderer descriptor handle.
   using GuiWidgetHandle = TypedHandle<GuiWidgetHandleTag>; ///< v5-internal GUI widget handle.

   using vve::Bounds;              ///< Public axis-aligned bounds contract.
   using vve::Camera;              ///< Public camera contract.
   using vve::ClipPlanes;          ///< Public clip-plane contract.
   using vve::DeltaTime;           ///< Public frame delta contract.
   using vve::Direction;           ///< Public direction contract.
   using vve::FovY;                ///< Public vertical field-of-view contract.
   using vve::FrameCount;          ///< Public frame-count contract.
   using vve::IndexCount;          ///< Public index-count contract.
   using vve::LightIntensity;      ///< Public light-intensity contract.
   using vve::LightRange;          ///< Public light-range contract.
   using vve::LinearColor;         ///< Public linear-color contract.
   using vve::ObjectName;          ///< Public object-name contract.
   using vve::PixelExtent;         ///< Public pixel-extent contract.
   using vve::Position;            ///< Public position contract.
   using vve::RendererId;          ///< Public renderer-id contract.
   using vve::Rotation;            ///< Public rotation contract.
   using vve::Scale;               ///< Public scale contract.
   using vve::SpotConeAngle;       ///< Public spotlight-cone contract.
   using vve::TextureChannelCount; ///< Public texture-channel-count contract.
   using vve::Transform;           ///< Public transform contract.
   using vve::VertexCount;         ///< Public vertex-count contract.

} // namespace vve::v5
