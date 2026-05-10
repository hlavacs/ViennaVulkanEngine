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

   struct RenderDebugSample {
      std::uint32_t vertex_id{};           ///< Source vertex id.
      Vec3 world{zeroVec3()};              ///< World-space vertex position.
      Vec4 clip{};                         ///< Clip-space position.
      Vec3 ndc{zeroVec3()};                ///< Normalized device coordinate.
      Vec3 normal{zeroVec3()};             ///< Normal used for lighting.
      Vec3 direction_to_light{zeroVec3()}; ///< Direction from surface to light.
      Vec3 ambient_lighting{zeroVec3()};   ///< Ambient light contribution.
      Vec3 direct_lighting{zeroVec3()};    ///< Direct light contribution.
      Vec3 final_lighting{zeroVec3()};     ///< Ambient plus direct lighting.
      float depth{};                       ///< Vulkan depth value.
      float n_dot_l{};                     ///< Lambert cosine term.
      bool valid{};                        ///< Whether this slot contains a sample.
   };

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
      [[nodiscard]] std::uint64_t sceneUploadCount() const;
      [[nodiscard]] std::uint64_t sceneMeshDrawCount() const;
      [[nodiscard]] std::uint64_t sceneInstanceDrawCount() const;
      [[nodiscard]] std::uint32_t sceneDrawVertexCount() const;
      [[nodiscard]] std::uint32_t sceneDrawIndexCount() const;
      [[nodiscard]] std::size_t sceneDebugSampleCount() const;
      [[nodiscard]] std::optional<RenderDebugSample> sceneCpuDebugSample(std::size_t index) const;
      [[nodiscard]] std::optional<RenderDebugSample> sceneGpuDebugSample(std::size_t index) const;
      [[nodiscard]] std::optional<float> sceneDebugClipError(std::size_t index) const;
      [[nodiscard]] std::optional<float> sceneDebugDepthError(std::size_t index) const;
      [[nodiscard]] std::optional<float> sceneDebugLightingError(std::size_t index) const;
      [[nodiscard]] std::size_t lastRenderedWindowCount() const { return impl_.lastRenderedWindowCount(); }
      [[nodiscard]] std::size_t preparedGpuTargetCount() const { return impl_.preparedGpuTargetCount(); }
      [[nodiscard]] std::array<float, 4> lastClearColor() const { return impl_.lastClearColor(); }

   private:
      Impl &impl_; ///< Selected implementation render system.
   }; ///< Public render-system wrapper.

   /// @brief Returns how many scene uploads completed.
   inline std::uint64_t RenderSystem::sceneUploadCount() const { return impl_.sceneUploadCount(); }

   /// @brief Returns how many source meshes were drawn by the uploaded scene path.
   inline std::uint64_t RenderSystem::sceneMeshDrawCount() const { return impl_.sceneMeshDrawCount(); }

   /// @brief Returns how many source instances were drawn by the uploaded scene path.
   inline std::uint64_t RenderSystem::sceneInstanceDrawCount() const { return impl_.sceneInstanceDrawCount(); }

   /// @brief Returns how many vertices were uploaded by the scene draw path.
   inline std::uint32_t RenderSystem::sceneDrawVertexCount() const { return impl_.sceneDrawVertexCount(); }

   /// @brief Returns how many indices were uploaded by the scene draw path.
   inline std::uint32_t RenderSystem::sceneDrawIndexCount() const { return impl_.sceneDrawIndexCount(); }

   /// @brief Returns how many debug sample slots are expected for the scene draw.
   inline std::size_t RenderSystem::sceneDebugSampleCount() const { return impl_.sceneDebugSampleCount(); }

   /// @brief Returns one CPU-computed render debug sample.
   inline std::optional<RenderDebugSample> RenderSystem::sceneCpuDebugSample(std::size_t index) const {
      auto sample = impl_.sceneCpuDebugSample(index);
      if (!sample) { return {}; }
      return RenderDebugSample{.vertex_id = sample->vertex_id, .world = sample->world, .clip = sample->clip,
                               .ndc = sample->ndc, .normal = sample->normal,
                               .direction_to_light = sample->direction_to_light,
                               .ambient_lighting = sample->ambient_lighting,
                               .direct_lighting = sample->direct_lighting,
                               .final_lighting = sample->final_lighting, .depth = sample->depth,
                               .n_dot_l = sample->n_dot_l, .valid = sample->valid};
   }

   /// @brief Returns one GPU-computed render debug sample.
   inline std::optional<RenderDebugSample> RenderSystem::sceneGpuDebugSample(std::size_t index) const {
      auto sample = impl_.sceneGpuDebugSample(index);
      if (!sample) { return {}; }
      return RenderDebugSample{.vertex_id = sample->vertex_id, .world = sample->world, .clip = sample->clip,
                               .ndc = sample->ndc, .normal = sample->normal,
                               .direction_to_light = sample->direction_to_light,
                               .ambient_lighting = sample->ambient_lighting,
                               .direct_lighting = sample->direct_lighting,
                               .final_lighting = sample->final_lighting, .depth = sample->depth,
                               .n_dot_l = sample->n_dot_l, .valid = sample->valid};
   }

   /// @brief Returns the CPU/GPU clip-space mismatch for one sample.
   inline std::optional<float> RenderSystem::sceneDebugClipError(std::size_t index) const {
      return impl_.sceneDebugClipError(index);
   }

   /// @brief Returns the CPU/GPU depth mismatch for one sample.
   inline std::optional<float> RenderSystem::sceneDebugDepthError(std::size_t index) const {
      return impl_.sceneDebugDepthError(index);
   }

   /// @brief Returns the CPU/GPU lighting-term mismatch for one sample.
   inline std::optional<float> RenderSystem::sceneDebugLightingError(std::size_t index) const {
      return impl_.sceneDebugLightingError(index);
   }

} // namespace vve
