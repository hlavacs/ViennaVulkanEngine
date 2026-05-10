export module VEEngine:RenderSystem;
import std;
import VEEngine.V4;
import VEEngine.Error;
import VEEngine.Types;

/**
 * @file
 * @brief Public render-system facade backed by the selected engine implementation.
 */
export namespace vve {

   class RenderSystem {
      using Impl = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::RenderSystem;

   public:
      explicit RenderSystem(Impl &implementation) : impl_{implementation} {}
      RenderSystem(const RenderSystem &) = default;
      RenderSystem(RenderSystem &&) noexcept = default;
      RenderSystem &operator=(const RenderSystem &) = delete;
      RenderSystem &operator=(RenderSystem &&) noexcept = delete;

      void clearScene() { impl_.clearScene(); }
      void setCamera(Camera camera, PixelExtent extent) { impl_.setCamera(std::move(camera), extent); }
      void setDirectionalLight(Direction direction_to_light, LinearColor color,
                               LightIntensity intensity, LinearColor ambient) {
         impl_.setDirectionalLight(direction_to_light, color, intensity, ambient);
      }
      [[nodiscard]] std::expected<void, Error> addPlane(Vec2 half_extent, LinearColor color,
                                                        Transform transform = {}) {
         return impl_.addPlane(half_extent, color, transform);
      }
      [[nodiscard]] std::expected<void, Error> addCuboid(Vec3 minimum, Vec3 maximum, LinearColor color,
                                                         Transform transform = {}) {
         return impl_.addCuboid(minimum, maximum, color, transform);
      }
      [[nodiscard]] std::size_t sceneMeshCount() const { return impl_.sceneMeshCount(); }
      [[nodiscard]] std::size_t sceneMaterialCount() const { return impl_.sceneMaterialCount(); }
      [[nodiscard]] std::size_t sceneInstanceCount() const { return impl_.sceneInstanceCount(); }
      [[nodiscard]] std::size_t sceneVertexCount() const { return impl_.sceneVertexCount(); }
      [[nodiscard]] std::size_t sceneIndexCount() const { return impl_.sceneIndexCount(); }
      [[nodiscard]] bool hasSceneCamera() const { return impl_.hasSceneCamera(); }
      [[nodiscard]] bool hasSceneDirectionalLight() const { return impl_.hasSceneDirectionalLight(); }
      [[nodiscard]] std::uint64_t renderedFrameCount() const { return impl_.renderedFrameCount(); }
      [[nodiscard]] std::uint64_t presentedFrameCount() const { return impl_.presentedFrameCount(); }
      [[nodiscard]] std::uint64_t triangleDrawCount() const { return impl_.triangleDrawCount(); }
      [[nodiscard]] std::uint32_t triangleVertexCount() const { return impl_.triangleVertexCount(); }
      [[nodiscard]] std::size_t lastRenderedWindowCount() const { return impl_.lastRenderedWindowCount(); }
      [[nodiscard]] std::size_t preparedGpuTargetCount() const { return impl_.preparedGpuTargetCount(); }
      [[nodiscard]] std::array<float, 4> lastClearColor() const { return impl_.lastClearColor(); }

   private:
      Impl &impl_; ///< Selected implementation render system.
   }; ///< Public render-system wrapper.

} // namespace vve
