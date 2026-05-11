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
      Vec4 light_clip{};                   ///< Directional-light clip-space position.
      Vec3 ndc{zeroVec3()};                ///< Normalized device coordinate.
      Vec3 light_ndc{zeroVec3()};          ///< Directional-light normalized device coordinate.
      Vec3 normal{zeroVec3()};             ///< Normal used for lighting.
      Vec3 direction_to_light{zeroVec3()}; ///< Direction from surface to light.
      Vec3 ambient_lighting{zeroVec3()};   ///< Ambient light contribution.
      Vec3 direct_lighting{zeroVec3()};    ///< Direct light contribution.
      Vec3 point_lighting{zeroVec3()};     ///< Point-light contribution.
      Vec3 spot_lighting{zeroVec3()};      ///< Spot-light contribution.
      Vec3 final_lighting{zeroVec3()};     ///< Ambient plus direct lighting.
      float depth{};                       ///< Vulkan depth value.
      float light_depth{};                 ///< Directional-light depth value.
      float sampled_shadow_depth{};        ///< Shadow-map depth sampled by the shader.
      float shadow_depth_delta{};          ///< Light depth minus sampled shadow depth.
      float shadow_bias{};                 ///< Bias used by the shadow comparison.
      float shadow_factor{};               ///< One when lit, zero when shadowed.
      float n_dot_l{};                     ///< Lambert cosine term.
      bool inside_light{};                 ///< Whether the sample is inside the light projection.
      bool valid{};                        ///< Whether this slot contains a sample.
   };

   /// @brief Public CPU/GPU comparison point for downloaded shadow-depth data.
   struct RenderShadowDepthSample {
      std::uint32_t triangle_id{}; ///< Source triangle used for the centroid sample.
      Vec3 world{zeroVec3()};      ///< World-space centroid.
      Vec3 light_ndc{zeroVec3()};  ///< Directional-light normalized device coordinate.
      std::uint32_t pixel_x{};     ///< Shadow-map texel x coordinate.
      std::uint32_t pixel_y{};     ///< Shadow-map texel y coordinate.
      float expected_depth{};      ///< CPU-computed light-space depth.
      float gpu_depth{};           ///< Downloaded shadow-map depth.
      float error{};               ///< Absolute CPU/GPU depth mismatch.
      bool has_gpu{};              ///< Whether the depth image was downloaded.
      bool valid{};                ///< Whether this slot contains a sample.
   };

   class RenderSystem {
      using Impl = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::RenderSystem;

   public:
      explicit RenderSystem(Impl &implementation);
      RenderSystem(const RenderSystem &) = default;
      RenderSystem(RenderSystem &&) noexcept = default;
      RenderSystem &operator=(const RenderSystem &) = delete;
      RenderSystem &operator=(RenderSystem &&) noexcept = delete;

      void clearScene();
      void setCamera(Camera camera, PixelExtent extent);
      void setDirectionalLight(Direction direction_to_light, LinearColor color,
                               LightIntensity intensity, LinearColor ambient);
      void setPointLight(Position position, LinearColor color, LightIntensity intensity, LightRange range);
      void setSpotLight(Position position, Direction direction, LinearColor color,
                        LightIntensity intensity, LightRange range, SpotConeAngle cone);
      [[nodiscard]] std::expected<void, Error> addPlane(Vec2 half_extent, LinearColor color,
                                                        Transform transform = {});
      [[nodiscard]] std::expected<void, Error> addCuboid(Vec3 minimum, Vec3 maximum, LinearColor color,
                                                         Transform transform = {});
      [[nodiscard]] std::size_t sceneMeshCount() const;
      [[nodiscard]] std::size_t sceneMaterialCount() const;
      [[nodiscard]] std::size_t sceneInstanceCount() const;
      [[nodiscard]] std::size_t sceneVertexCount() const;
      [[nodiscard]] std::size_t sceneIndexCount() const;
      [[nodiscard]] bool hasSceneCamera() const;
      [[nodiscard]] bool hasSceneDirectionalLight() const;
      [[nodiscard]] bool hasScenePointLight() const;
      [[nodiscard]] bool hasSceneSpotLight() const;
      [[nodiscard]] std::uint64_t renderedFrameCount() const;
      [[nodiscard]] std::uint64_t presentedFrameCount() const;
      [[nodiscard]] std::uint64_t triangleDrawCount() const;
      [[nodiscard]] std::uint32_t triangleVertexCount() const;
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
      [[nodiscard]] std::optional<float> sceneDebugLightSpaceError(std::size_t index) const;
      [[nodiscard]] std::optional<float> sceneDebugLightingError(std::size_t index) const;
      [[nodiscard]] std::optional<float> sceneDebugShadowSampleError(std::size_t index) const;
      [[nodiscard]] std::size_t sceneShadowDepthSampleCount() const;
      [[nodiscard]] std::optional<RenderShadowDepthSample> sceneShadowDepthSample(std::size_t index) const;
      [[nodiscard]] std::optional<float> sceneShadowDepthError(std::size_t index) const;
      [[nodiscard]] std::size_t lastRenderedWindowCount() const;
      [[nodiscard]] std::size_t preparedGpuTargetCount() const;
      [[nodiscard]] std::array<float, 4> lastClearColor() const;

   private:
      Impl &impl_; ///< Selected implementation render system.
   }; ///< Public render-system wrapper.

   /// @brief Wraps the selected render-system implementation.
   inline RenderSystem::RenderSystem(Impl &implementation) : impl_{implementation} {}

   /// @brief Clears the active CPU scene.
   inline void RenderSystem::clearScene() { impl_.clearScene(); }

   /// @brief Sets the active scene camera.
   inline void RenderSystem::setCamera(Camera camera, PixelExtent extent) { impl_.setCamera(std::move(camera), extent); }

   /// @brief Sets the active directional light.
   inline void RenderSystem::setDirectionalLight(Direction direction_to_light, LinearColor color,
                                                 LightIntensity intensity, LinearColor ambient) {
      impl_.setDirectionalLight(direction_to_light, color, intensity, ambient);
   }

   /// @brief Sets the active point light.
   inline void RenderSystem::setPointLight(Position position, LinearColor color,
                                           LightIntensity intensity, LightRange range) {
      impl_.setPointLight(position, color, intensity, range);
   }

   /// @brief Sets the active spotlight.
   inline void RenderSystem::setSpotLight(Position position, Direction direction, LinearColor color,
                                          LightIntensity intensity, LightRange range, SpotConeAngle cone) {
      impl_.setSpotLight(position, direction, color, intensity, range, cone);
   }

   /// @brief Adds a colored plane to the active CPU scene.
   inline std::expected<void, Error> RenderSystem::addPlane(Vec2 half_extent, LinearColor color,
                                                            Transform transform) {
      return impl_.addPlane(half_extent, color, transform);
   }

   /// @brief Adds a colored cuboid to the active CPU scene.
   inline std::expected<void, Error> RenderSystem::addCuboid(Vec3 minimum, Vec3 maximum, LinearColor color,
                                                             Transform transform) {
      return impl_.addCuboid(minimum, maximum, color, transform);
   }

   /// @brief Returns mesh count in the active CPU scene.
   inline std::size_t RenderSystem::sceneMeshCount() const { return impl_.sceneMeshCount(); }

   /// @brief Returns material count in the active CPU scene.
   inline std::size_t RenderSystem::sceneMaterialCount() const { return impl_.sceneMaterialCount(); }

   /// @brief Returns instance count in the active CPU scene.
   inline std::size_t RenderSystem::sceneInstanceCount() const { return impl_.sceneInstanceCount(); }

   /// @brief Returns source vertex count in the active CPU scene.
   inline std::size_t RenderSystem::sceneVertexCount() const { return impl_.sceneVertexCount(); }

   /// @brief Returns source index count in the active CPU scene.
   inline std::size_t RenderSystem::sceneIndexCount() const { return impl_.sceneIndexCount(); }

   /// @brief Reports whether the active CPU scene has a camera.
   inline bool RenderSystem::hasSceneCamera() const { return impl_.hasSceneCamera(); }

   /// @brief Reports whether the active CPU scene has a directional light.
   inline bool RenderSystem::hasSceneDirectionalLight() const { return impl_.hasSceneDirectionalLight(); }

   /// @brief Reports whether the active CPU scene has a point light.
   inline bool RenderSystem::hasScenePointLight() const { return impl_.hasScenePointLight(); }

   /// @brief Reports whether the active CPU scene has a spotlight.
   inline bool RenderSystem::hasSceneSpotLight() const { return impl_.hasSceneSpotLight(); }

   /// @brief Returns the number of frames accepted by the render system.
   inline std::uint64_t RenderSystem::renderedFrameCount() const { return impl_.renderedFrameCount(); }

   /// @brief Returns the number of frames presented by the render system.
   inline std::uint64_t RenderSystem::presentedFrameCount() const { return impl_.presentedFrameCount(); }

   /// @brief Returns the number of smoke-test triangle draws.
   inline std::uint64_t RenderSystem::triangleDrawCount() const { return impl_.triangleDrawCount(); }

   /// @brief Returns the number of smoke-test triangle vertices.
   inline std::uint32_t RenderSystem::triangleVertexCount() const { return impl_.triangleVertexCount(); }

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
                               .light_clip = sample->light_clip, .ndc = sample->ndc,
                               .light_ndc = sample->light_ndc, .normal = sample->normal,
                               .direction_to_light = sample->direction_to_light,
                               .ambient_lighting = sample->ambient_lighting,
                               .direct_lighting = sample->direct_lighting,
                               .point_lighting = sample->point_lighting,
                               .spot_lighting = sample->spot_lighting,
                               .final_lighting = sample->final_lighting, .depth = sample->depth,
                               .light_depth = sample->light_depth,
                               .sampled_shadow_depth = sample->sampled_shadow_depth,
                               .shadow_depth_delta = sample->shadow_depth_delta,
                               .shadow_bias = sample->shadow_bias, .shadow_factor = sample->shadow_factor,
                               .n_dot_l = sample->n_dot_l,
                               .inside_light = sample->inside_light, .valid = sample->valid};
   }

   /// @brief Returns one GPU-computed render debug sample.
   inline std::optional<RenderDebugSample> RenderSystem::sceneGpuDebugSample(std::size_t index) const {
      auto sample = impl_.sceneGpuDebugSample(index);
      if (!sample) { return {}; }
      return RenderDebugSample{.vertex_id = sample->vertex_id, .world = sample->world, .clip = sample->clip,
                               .light_clip = sample->light_clip, .ndc = sample->ndc,
                               .light_ndc = sample->light_ndc, .normal = sample->normal,
                               .direction_to_light = sample->direction_to_light,
                               .ambient_lighting = sample->ambient_lighting,
                               .direct_lighting = sample->direct_lighting,
                               .point_lighting = sample->point_lighting,
                               .spot_lighting = sample->spot_lighting,
                               .final_lighting = sample->final_lighting, .depth = sample->depth,
                               .light_depth = sample->light_depth,
                               .sampled_shadow_depth = sample->sampled_shadow_depth,
                               .shadow_depth_delta = sample->shadow_depth_delta,
                               .shadow_bias = sample->shadow_bias, .shadow_factor = sample->shadow_factor,
                               .n_dot_l = sample->n_dot_l,
                               .inside_light = sample->inside_light, .valid = sample->valid};
   }

   /// @brief Returns the CPU/GPU clip-space mismatch for one sample.
   inline std::optional<float> RenderSystem::sceneDebugClipError(std::size_t index) const {
      return impl_.sceneDebugClipError(index);
   }

   /// @brief Returns the CPU/GPU depth mismatch for one sample.
   inline std::optional<float> RenderSystem::sceneDebugDepthError(std::size_t index) const {
      return impl_.sceneDebugDepthError(index);
   }

   /// @brief Returns the CPU/GPU directional-light-space mismatch for one sample.
   inline std::optional<float> RenderSystem::sceneDebugLightSpaceError(std::size_t index) const {
      return impl_.sceneDebugLightSpaceError(index);
   }

   /// @brief Returns the CPU/GPU lighting-term mismatch for one sample.
   inline std::optional<float> RenderSystem::sceneDebugLightingError(std::size_t index) const {
      return impl_.sceneDebugLightingError(index);
   }

   /// @brief Returns the shader-sampled shadow depth mismatch against the copied shadow map.
   inline std::optional<float> RenderSystem::sceneDebugShadowSampleError(std::size_t index) const {
      return impl_.sceneDebugShadowSampleError(index);
   }

   /// @brief Returns how many shadow-depth proof samples are available.
   inline std::size_t RenderSystem::sceneShadowDepthSampleCount() const {
      return impl_.sceneShadowDepthSampleCount();
   }

   /// @brief Returns one downloaded shadow-depth proof sample.
   inline std::optional<RenderShadowDepthSample> RenderSystem::sceneShadowDepthSample(std::size_t index) const {
      const auto sample = impl_.sceneShadowDepthSample(index);
      if (!sample) { return {}; }
      return RenderShadowDepthSample{.triangle_id = sample->triangle_id,
                                     .world = sample->world,
                                     .light_ndc = sample->light_ndc,
                                     .pixel_x = sample->pixel_x,
                                     .pixel_y = sample->pixel_y,
                                     .expected_depth = sample->expected_depth,
                                     .gpu_depth = sample->gpu_depth,
                                     .error = sample->error,
                                     .has_gpu = sample->has_gpu,
                                     .valid = sample->valid};
   }

   /// @brief Returns the CPU/GPU shadow-depth mismatch for one proof sample.
   inline std::optional<float> RenderSystem::sceneShadowDepthError(std::size_t index) const {
      return impl_.sceneShadowDepthError(index);
   }

   /// @brief Returns how many visible windows were considered by the last frame.
   inline std::size_t RenderSystem::lastRenderedWindowCount() const { return impl_.lastRenderedWindowCount(); }

   /// @brief Returns how many Vulkan targets are prepared.
   inline std::size_t RenderSystem::preparedGpuTargetCount() const { return impl_.preparedGpuTargetCount(); }

   /// @brief Returns the last clear color used by the renderer.
   inline std::array<float, 4> RenderSystem::lastClearColor() const { return impl_.lastClearColor(); }

} // namespace vve
