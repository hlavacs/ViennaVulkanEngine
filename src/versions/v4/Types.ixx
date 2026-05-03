export module VEEngine.V4:Types;
import std;
export import :ECS;
export import VEEngine;

/// @file
/// @brief v4 aliases for facade data plus v4-internal graph/resource handle types.

export namespace vve::v4 {

   namespace math = ::vve::math; ///< Version-local alias for the shared math namespace.

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

   using ::vve::Bounds;
   using ::vve::Camera;
   using ::vve::CameraDescriptor;
   using ::vve::CameraHandle;
   using ::vve::ClipPlanes;
   using ::vve::DeltaTime;
   using ::vve::DescriptorMap;
   using ::vve::Direction;
   using ::vve::FovY;
   using ::vve::FrameCount;
   using ::vve::IndexCount;
   using ::vve::LightDescriptor;
   using ::vve::LightHandle;
   using ::vve::LightIntensity;
   using ::vve::LightKind;
   using ::vve::LinearColor;
   using ::vve::MaterialDescriptor;
   using ::vve::MaterialHandle;
   using ::vve::MeshDescriptor;
   using ::vve::MeshHandle;
   using ::vve::MeshUse;
   using ::vve::NodeDescriptor;
   using ::vve::NodeHandle;
   using ::vve::ObjectCatalog;
   using ::vve::ObjectName;
   using ::vve::PixelExtent;
   using ::vve::Position;
   using ::vve::RendererId;
   using ::vve::Rotation;
   using ::vve::Scale;
   using ::vve::SceneDescriptor;
   using ::vve::SceneHandle;
   using ::vve::TextureBinding;
   using ::vve::TextureChannelCount;
   using ::vve::TextureDescriptor;
   using ::vve::TextureHandle;
   using ::vve::TextureSemantic;
   using ::vve::Transform;
   using ::vve::Tree;
   using ::vve::VertexCount;

   template <typename THandle> using BasicTree = ::vve::BasicTree<THandle>; ///< Facade tree topology.

   using ResourceHandle   = TypedHandle<decltype([] {})>; ///< v4-internal resource handle.
   using ShaderHandle     = TypedHandle<decltype([] {})>; ///< v4-internal shader descriptor handle.
   using TaskHandle       = TypedHandle<decltype([] {})>; ///< v4-internal task descriptor handle.
   using RenderPassHandle = TypedHandle<decltype([] {})>; ///< v4-internal render-pass handle.
   using RendererHandle   = TypedHandle<decltype([] {})>; ///< v4-internal renderer descriptor handle.
   using GuiWidgetHandle  = TypedHandle<decltype([] {})>; ///< v4-internal GUI widget handle.

   /// @brief Generic directed graph topology used by v4 task and render graphs.
   template <typename THandle> struct Graph {
      std::multimap<THandle, THandle> edges{}; ///< Source node handle mapped to destination node handles.

      /// @brief Adds one directed edge.
      void addEdge(THandle from, THandle to) { edges.emplace(from, to); }

      /// @brief Returns all outgoing edges for a node handle.
      [[nodiscard]] auto childRange(THandle node) const { return edges.equal_range(node); }

      /// @brief Returns nodes in dependency order, or cycle_detected when the graph is cyclic.
      [[nodiscard]] std::expected<Vector<THandle>, Error> topologicalOrder(const Vector<THandle> &nodes) const {
         std::map<THandle, std::uint32_t> incoming{};
         std::map<THandle, Vector<THandle>> outgoing{};
         for (const auto node : nodes) {
            if (!node.valid()) { return std::unexpected(Error::invalid_handle); }
            incoming.try_emplace(node, 0);
         }

         for (const auto &[from, to] : edges) {
            if (!from.valid() || !to.valid()) { return std::unexpected(Error::invalid_handle); }
            if (!incoming.contains(from) || !incoming.contains(to)) { return std::unexpected(Error::missing_object); }
            outgoing[from].push_back(to);
            ++incoming[to];
         }

         std::set<THandle> ready{};
         Vector<THandle> ordered{};
         ordered.reserve(incoming.size());
         for (const auto &[node, count] : incoming) {
            if (count == 0) { ready.insert(node); }
         }

         while (!ready.empty()) {
            const auto node = *ready.begin();
            ready.erase(ready.begin());
            ordered.push_back(node);
            for (const auto child : outgoing[node]) {
               auto &count = incoming[child];
               if (--count == 0) { ready.insert(child); }
            }
         }

         if (ordered.size() != incoming.size()) { return std::unexpected(Error::cycle_detected); }
         return ordered;
      }

   };

} // namespace vve::v4
