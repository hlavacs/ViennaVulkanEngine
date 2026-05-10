module;

#ifndef VVE_V4_SHADER_SOURCE_DIR
#define VVE_V4_SHADER_SOURCE_DIR "src/versions/v4/shaders"
#endif

export module VEEngine.V4:RenderSystem;
import std;
export import :RenderPass;
import :RendererForward;
import :Shaders;
import :Vulkan;
import :Window;

/// @file
/// @brief CPU render data and renderer selection system.

export namespace vve::v4 {

   using RenderMeshHandle     = TypedHandle<decltype([] {})>; ///< v4 render mesh handle.
   using RenderMaterialHandle = TypedHandle<decltype([] {})>; ///< v4 render material handle.
   using RenderInstanceHandle = TypedHandle<decltype([] {})>; ///< v4 render instance handle.

} // namespace vve::v4

namespace vve::v4::detail { struct RendererChoice; }

export namespace vve::v4 {

   /// @brief Vertex payload accepted by the first forward renderer milestone.
   struct RenderVertex {
      Vec3 position{zeroVec3()};                ///< Object-space position.
      Vec3 normal{Vec3(zero(), one(), zero())}; ///< Object-space normal.
      Vec2 uv{zero(), zero()};                  ///< First texture coordinate.
   };

   /// @brief CPU-side mesh data ready to become Vulkan vertex and index buffers.
   struct RenderMesh {
      RenderMeshHandle handle{};       ///< Stable render mesh handle.
      Vector<RenderVertex> vertices{}; ///< Source vertices.
      Vector<std::uint32_t> indices{}; ///< Triangle indices.
      Bounds bounds{};                 ///< Object-space bounds.
   };

   /// @brief CPU-side material data for a simple forward pass.
   struct RenderMaterial {
      RenderMaterialHandle handle{};                     ///< Stable render material handle.
      LinearColor base_color{.value = oneVec3()};        ///< Base color factor.
      TextureHandle base_color_texture{};                ///< Optional texture handle; invalid means none.
      std::filesystem::path base_color_texture_source{}; ///< Optional source path for diagnostics.
   };

   /// @brief One draw item connecting mesh, material, and transforms.
   struct RenderInstance {
      RenderInstanceHandle handle{};        ///< Stable render instance handle.
      RenderMeshHandle mesh{};              ///< Mesh drawn by this instance.
      RenderMaterialHandle material{};      ///< Material used by this instance.
      Transform local_transform{};          ///< Source scene local transform.
      Mat4 world_transform{identityMat4()}; ///< World transform used by rendering.
   };

   /// @brief One-light model used by the first shadow-map milestone.
   struct RenderDirectionalLight {
      Direction direction_to_light{.value = Vec3(-0.5F, 1.0F, 0.25F)}; ///< Direction from surface to light.
      LinearColor color{.value = oneVec3()};                           ///< Direct light color.
      LightIntensity intensity{.value = one()};                        ///< Direct light intensity.
      LinearColor ambient{.value = Vec3(0.04F, 0.04F, 0.04F)};          ///< Small ambient term.
      Mat4 light_view_projection{identityMat4()};                      ///< Future shadow-map light matrix.
   };

   /// @brief Camera data consumed by render passes.
   struct RenderCamera {
      Camera camera{};                                    ///< Facade camera description.
      PixelExtent target_extent{.width = 1, .height = 1}; ///< Render target size.
   };

   /// @brief CPU or GPU camera-transform sample used by the verification example.
   struct RenderDebugSample {
      std::uint32_t vertex_id{}; ///< Source vertex id.
      Vec3 world{zeroVec3()};    ///< World-space vertex position.
      Vec4 clip{};               ///< Clip-space position.
      Vec3 ndc{zeroVec3()};      ///< Normalized device coordinate.
      float depth{};             ///< Vulkan depth value.
      bool valid{};              ///< Whether this slot contains a sample.
   };

   /// @brief Minimal CPU render scene built before Vulkan upload.
   class RenderScene {
   public:
      [[nodiscard]] RenderMaterialHandle addMaterial(RenderMaterial material = {});
      [[nodiscard]] RenderMeshHandle addMesh(Vector<RenderVertex> vertices, Vector<std::uint32_t> indices,
                                             Bounds bounds = {});
      [[nodiscard]] RenderMeshHandle addPlaneMesh(Vec2 half_extent);
      [[nodiscard]] RenderMeshHandle addCuboidMesh(Vec3 minimum, Vec3 maximum);
      [[nodiscard]] std::expected<RenderInstanceHandle, Error>
      addInstance(RenderMeshHandle mesh, RenderMaterialHandle material, Transform local = {},
                  Mat4 world = identityMat4());
      void setCamera(RenderCamera camera);
      void setDirectionalLight(RenderDirectionalLight light);
      void clear();
      [[nodiscard]] const RenderMesh *findMesh(RenderMeshHandle handle) const;
      [[nodiscard]] const RenderMaterial *findMaterial(RenderMaterialHandle handle) const;
      [[nodiscard]] const RenderInstance *findInstance(RenderInstanceHandle handle) const;
      [[nodiscard]] std::size_t meshCount() const;
      [[nodiscard]] std::size_t materialCount() const;
      [[nodiscard]] std::size_t instanceCount() const;
      [[nodiscard]] std::size_t vertexCount() const;
      [[nodiscard]] std::size_t indexCount() const;
      [[nodiscard]] const std::optional<RenderCamera> &camera() const;
      [[nodiscard]] const std::optional<RenderDirectionalLight> &directionalLight() const;
      [[nodiscard]] const Vector<RenderInstance> &instances() const;

   private:
      static void appendFace(Vector<RenderVertex> &vertices, Vector<std::uint32_t> &indices,
                             Vec3 normal, std::array<Vec3, 4> corners);

      Vector<RenderMesh> meshes_{};                   ///< CPU meshes awaiting upload.
      Vector<RenderMaterial> materials_{};            ///< CPU materials awaiting descriptor creation.
      Vector<RenderInstance> instances_{};            ///< Draw items.
      std::optional<RenderCamera> camera_{};          ///< Optional active camera.
      std::optional<RenderDirectionalLight> light_{}; ///< Optional active directional light.
   };

   /// @brief Renderer choice descriptor returned by RenderSystem.
   struct RendererDescriptor {
      using HandleType = RendererHandle;            ///< Descriptor handle type.
      RendererHandle handle{};                      ///< Stable renderer descriptor handle.
      RendererId id{.value = "forward"};            ///< Renderer id chosen by the application.
      bool shadow_maps{true};                       ///< Whether this renderer intends to use shadow maps.
      std::span<const RenderPassContract> passes{}; ///< Renderer-owned nodes and explicit dependencies.
   };

   /// @brief Minimal renderer factory and render-pass DAG builder.
   class RenderSystem {
   public:
      [[nodiscard]] std::expected<RendererDescriptor, Error> createRenderer(RendererId id) const;
      [[nodiscard]] RendererDescriptor createForwardRenderer() const;
      [[nodiscard]] std::expected<RenderGraph, Error> buildRenderGraph(const RendererDescriptor &renderer) const;
      [[nodiscard]] std::expected<RenderGraph, Error>
      buildRenderGraph(std::span<const RenderPassContract> passes) const;
      [[nodiscard]] std::expected<RenderGraph, Error>
      buildRenderGraph(std::span<const std::span<const RenderPassContract>> pass_lists) const;
      [[nodiscard]] std::expected<void, Error> addPlane(Vec2 half_extent, LinearColor color,
                                                        Transform transform = {});
      [[nodiscard]] std::expected<void, Error> addCuboid(Vec3 minimum, Vec3 maximum, LinearColor color,
                                                         Transform transform = {});
      void clearScene();
      void setCamera(Camera camera, PixelExtent extent);
      void setDirectionalLight(Direction direction_to_light, LinearColor color,
                               LightIntensity intensity, LinearColor ambient);
      [[nodiscard]] std::size_t sceneMeshCount() const;
      [[nodiscard]] std::size_t sceneMaterialCount() const;
      [[nodiscard]] std::size_t sceneInstanceCount() const;
      [[nodiscard]] std::size_t sceneVertexCount() const;
      [[nodiscard]] std::size_t sceneIndexCount() const;
      [[nodiscard]] bool hasSceneCamera() const;
      [[nodiscard]] bool hasSceneDirectionalLight() const;
      [[nodiscard]] std::expected<void, Error> renderFrame(const WindowFrameData &windows);
      [[nodiscard]] std::expected<void, Error> renderFrame(WindowSystem &windows);
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
      [[nodiscard]] std::size_t lastRenderedWindowCount() const;
      [[nodiscard]] std::size_t preparedGpuTargetCount() const;
      [[nodiscard]] std::array<float, 4> lastClearColor() const;

   private:
      [[nodiscard]] std::expected<void, Error> ensureTriangleShader();
      [[nodiscard]] std::expected<void, Error> ensureSceneShader();
      [[nodiscard]] std::expected<void, Error> buildSceneDrawData();
      [[nodiscard]] static std::expected<void, Error>
      addPass(RenderGraph &graph, std::map<std::string_view, RenderPassHandle> &handles,
              const RenderPassContract &pass);
      [[nodiscard]] static std::expected<void, Error>
      addDependencies(RenderGraph &graph, const std::map<std::string_view, RenderPassHandle> &handles,
                      std::span<const RenderPassContract> passes);
      [[nodiscard]] static RendererDescriptor createDescriptor(detail::RendererChoice choice);

      RenderScene scene_{};                    ///< Active CPU render scene awaiting GPU upload.
      vh::FrameHost vulkan_{};                 ///< Visible-window Vulkan frame targets.
      ShaderSystem shaders_{};                 ///< Built-in renderer shader compiler/cache.
      std::optional<ShaderHandle> triangle_shader_{}; ///< Compiled smoke-test triangle shader.
      std::optional<ShaderHandle> scene_shader_{}; ///< Compiled uploaded-scene unlit shader.
      std::vector<std::uint32_t> triangle_vertex_spirv_{}; ///< Cached triangle vertex SPIR-V.
      std::vector<std::uint32_t> triangle_fragment_spirv_{}; ///< Cached triangle fragment SPIR-V.
      std::vector<std::uint32_t> scene_vertex_spirv_{}; ///< Cached unlit scene vertex SPIR-V.
      std::vector<std::uint32_t> scene_fragment_spirv_{}; ///< Cached unlit scene fragment SPIR-V.
      std::vector<vh::SceneVertex> scene_vertices_{}; ///< Flattened debug scene vertices.
      std::vector<std::uint32_t> scene_indices_{}; ///< Flattened debug scene indices.
      std::array<float, 16> scene_clip_from_world_{}; ///< Column-major world-to-clip camera matrix.
      std::array<RenderDebugSample, 2> scene_cpu_debug_{}; ///< CPU camera-transform samples.
      std::uint64_t rendered_frames_{0};       ///< Number of frame hooks reached.
      std::size_t last_rendered_window_count_{0}; ///< Last non-closed window count.
   };

} // namespace vve::v4

namespace vve::v4::detail {

   /// @brief One row in the renderer registry.
   struct RendererChoice {
      std::string_view id{};                        ///< Renderer selector id.
      bool shadow_maps{};                           ///< Whether the renderer uses shadow maps.
      std::span<const RenderPassContract> passes{}; ///< Concrete render graph nodes.
   };

   inline constexpr std::array renderer_choices{ ///< Compile-time renderer registry.
       RendererChoice{.id = RendererForward::id(),
                      .shadow_maps = RendererForward::usesShadowMaps(),
                      .passes = RendererForward::passes()}};

} // namespace vve::v4::detail

namespace vve::v4 {

   namespace detail {

      inline constexpr std::array clear_color{0.10F, 0.20F, 0.30F, 1.00F}; ///< First GPU-frame proof color.

      /// @brief Builds the Vulkan clip transform used by the unlit scene proof renderer.
      inline Mat4 clipFromWorld(const RenderCamera &camera) {
         const auto width = static_cast<Scalar>(camera.target_extent.width);
         const auto height = static_cast<Scalar>(camera.target_extent.height == 0 ? 1 : camera.target_extent.height);
         const auto projection = math::perspectiveVulkan(camera.camera.fov_y.radians, width / height,
                                                         camera.camera.clip.near_plane,
                                                         camera.camera.clip.far_plane);
         return math::multiply(projection, camera.camera.view_transform);
      }

      /// @brief Stores a GLM-style column-major matrix as push-constant floats.
      inline std::array<float, 16> columns(Mat4 matrix) {
         auto result = std::array<float, 16>{};
         for (std::size_t column{}; column < 4; ++column) {
            for (std::size_t row{}; row < 4; ++row) {
               result[(column * 4) + row] = static_cast<float>(matrix[column][row]);
            }
         }
         return result;
      }

      /// @brief Converts a clip position to Vulkan normalized device coordinates.
      inline Vec3 ndc(Vec4 clip) {
         const auto inverse_w = std::abs(clip.w) > 1.0e-6F ? 1.0F / clip.w : 0.0F;
         return Vec3{clip.x * inverse_w, clip.y * inverse_w, clip.z * inverse_w};
      }

      /// @brief Builds the CPU-side sample equivalent to the GPU shader debug write.
      inline RenderDebugSample cpuSample(std::uint32_t vertex_id, const vh::SceneVertex &vertex, Mat4 clip_from_world) {
         const auto world = Vec3{vertex.x, vertex.y, vertex.z};
         const auto clip = math::multiply(clip_from_world, Vec4{world.x, world.y, world.z, 1.0F});
         const auto sample_ndc = ndc(clip);
         return RenderDebugSample{.vertex_id = vertex_id,
                                  .world = world,
                                  .clip = clip,
                                  .ndc = sample_ndc,
                                  .depth = sample_ndc.z,
                                  .valid = true};
      }

      /// @brief Converts the compact Vulkan readback sample to the render-system debug type.
      inline RenderDebugSample gpuSample(const vh::SceneDebugSample &sample) {
         const auto world = Vec3{sample.world[0], sample.world[1], sample.world[2]};
         const auto clip = Vec4{sample.clip[0], sample.clip[1], sample.clip[2], sample.clip[3]};
         const auto sample_ndc = Vec3{sample.ndc[0], sample.ndc[1], sample.ndc[2]};
         return RenderDebugSample{.vertex_id = sample.vertex_id,
                                  .world = world,
                                  .clip = clip,
                                  .ndc = sample_ndc,
                                  .depth = sample.depth,
                                  .valid = sample.vertex_id != vh::invalid_scene_debug_vertex};
      }

      /// @brief Computes the maximum absolute clip-coordinate difference.
      inline float clipError(const RenderDebugSample &cpu, const RenderDebugSample &gpu) {
         return std::max({std::abs(cpu.clip.x - gpu.clip.x), std::abs(cpu.clip.y - gpu.clip.y),
                          std::abs(cpu.clip.z - gpu.clip.z), std::abs(cpu.clip.w - gpu.clip.w)});
      }

   } // namespace detail

   /// @brief Adds a material and returns its stable handle.
   inline RenderMaterialHandle RenderScene::addMaterial(RenderMaterial material) {
      material.handle = makeCounterHandle<RenderMaterialHandle>();
      materials_.push_back(std::move(material));
      return materials_.back().handle;
   }

   /// @brief Adds mesh source data and returns its stable handle.
   inline RenderMeshHandle RenderScene::addMesh(Vector<RenderVertex> vertices, Vector<std::uint32_t> indices,
                                                Bounds bounds) {
      auto mesh = RenderMesh{.handle = makeCounterHandle<RenderMeshHandle>(),
                             .vertices = std::move(vertices),
                             .indices = std::move(indices),
                             .bounds = bounds};
      meshes_.push_back(std::move(mesh));
      return meshes_.back().handle;
   }

   /// @brief Adds a y-up plane mesh centered at the local origin.
   inline RenderMeshHandle RenderScene::addPlaneMesh(Vec2 half_extent) {
      const auto hx = half_extent.x;
      const auto hz = half_extent.y;
      const auto up = Vec3(zero(), one(), zero());
      auto vertices = Vector<RenderVertex>{
          RenderVertex{.position = Vec3(-hx, zero(), -hz), .normal = up, .uv = Vec2{0.0F, 0.0F}},
          RenderVertex{.position = Vec3( hx, zero(), -hz), .normal = up, .uv = Vec2{1.0F, 0.0F}},
          RenderVertex{.position = Vec3( hx, zero(),  hz), .normal = up, .uv = Vec2{1.0F, 1.0F}},
          RenderVertex{.position = Vec3(-hx, zero(),  hz), .normal = up, .uv = Vec2{0.0F, 1.0F}}};
      auto indices = Vector<std::uint32_t>{0, 1, 2, 0, 2, 3};
      return addMesh(std::move(vertices), std::move(indices),
                     Bounds{.minimum = Position{.value = Vec3(-hx, zero(), -hz)},
                            .maximum = Position{.value = Vec3( hx, zero(),  hz)}, .valid = true});
   }

   /// @brief Adds a cuboid mesh with separate face vertices so each face keeps a clean normal.
   inline RenderMeshHandle RenderScene::addCuboidMesh(Vec3 minimum, Vec3 maximum) {
      auto vertices = Vector<RenderVertex>{};
      auto indices = Vector<std::uint32_t>{};
      vertices.reserve(24);
      indices.reserve(36);

      appendFace(vertices, indices, Vec3( zero(),  zero(),  one()), {Vec3(minimum.x, minimum.y, maximum.z),
                 Vec3(maximum.x, minimum.y, maximum.z), Vec3(maximum.x, maximum.y, maximum.z),
                 Vec3(minimum.x, maximum.y, maximum.z)});
      appendFace(vertices, indices, Vec3( zero(),  zero(), -one()), {Vec3(maximum.x, minimum.y, minimum.z),
                 Vec3(minimum.x, minimum.y, minimum.z), Vec3(minimum.x, maximum.y, minimum.z),
                 Vec3(maximum.x, maximum.y, minimum.z)});
      appendFace(vertices, indices, Vec3( one(),   zero(),  zero()), {Vec3(maximum.x, minimum.y, maximum.z),
                 Vec3(maximum.x, minimum.y, minimum.z), Vec3(maximum.x, maximum.y, minimum.z),
                 Vec3(maximum.x, maximum.y, maximum.z)});
      appendFace(vertices, indices, Vec3(-one(),   zero(),  zero()), {Vec3(minimum.x, minimum.y, minimum.z),
                 Vec3(minimum.x, minimum.y, maximum.z), Vec3(minimum.x, maximum.y, maximum.z),
                 Vec3(minimum.x, maximum.y, minimum.z)});
      appendFace(vertices, indices, Vec3( zero(),  one(),   zero()), {Vec3(minimum.x, maximum.y, maximum.z),
                 Vec3(maximum.x, maximum.y, maximum.z), Vec3(maximum.x, maximum.y, minimum.z),
                 Vec3(minimum.x, maximum.y, minimum.z)});
      appendFace(vertices, indices, Vec3( zero(), -one(),   zero()), {Vec3(minimum.x, minimum.y, minimum.z),
                 Vec3(maximum.x, minimum.y, minimum.z), Vec3(maximum.x, minimum.y, maximum.z),
                 Vec3(minimum.x, minimum.y, maximum.z)});

      return addMesh(std::move(vertices), std::move(indices),
                     Bounds{.minimum = Position{.value = minimum},
                            .maximum = Position{.value = maximum}, .valid = true});
   }

   /// @brief Appends one independent quad face as two triangles.
   inline void RenderScene::appendFace(Vector<RenderVertex> &vertices, Vector<std::uint32_t> &indices,
                                       Vec3 normal, std::array<Vec3, 4> corners) {
      const auto base = static_cast<std::uint32_t>(vertices.size());
      const std::array uvs{Vec2{0.0F, 0.0F}, Vec2{1.0F, 0.0F}, Vec2{1.0F, 1.0F}, Vec2{0.0F, 1.0F}};
      for (std::size_t i{}; i < corners.size(); ++i) {
         vertices.push_back(RenderVertex{.position = corners[i], .normal = normal, .uv = uvs[i]});
      }
      for (const auto index : std::array{0U, 1U, 2U, 0U, 2U, 3U}) { indices.push_back(base + index); }
   }

   /// @brief Adds one render instance if mesh and material handles are known.
   inline std::expected<RenderInstanceHandle, Error>
   RenderScene::addInstance(RenderMeshHandle mesh, RenderMaterialHandle material, Transform local, Mat4 world) {
      if (!findMesh(mesh) || !findMaterial(material)) { return std::unexpected(Error::missing_object); }
      auto instance = RenderInstance{.handle = makeCounterHandle<RenderInstanceHandle>(),
                                     .mesh = mesh,
                                     .material = material,
                                     .local_transform = local,
                                     .world_transform = world};
      instances_.push_back(std::move(instance));
      return instances_.back().handle;
   }

   /// @brief Sets the active render camera.
   inline void RenderScene::setCamera(RenderCamera camera) { camera_ = std::move(camera); }

   /// @brief Sets the active directional light.
   inline void RenderScene::setDirectionalLight(RenderDirectionalLight light) { light_ = std::move(light); }

   /// @brief Clears CPU render scene state.
   inline void RenderScene::clear() { meshes_ = {}; materials_ = {}; instances_ = {}; camera_ = {}; light_ = {}; }

   /// @brief Finds one mesh by handle.
   inline const RenderMesh *RenderScene::findMesh(RenderMeshHandle handle) const {
      const auto it = std::ranges::find(meshes_, handle, &RenderMesh::handle);
      return it == meshes_.end() ? nullptr : std::addressof(*it);
   }

   /// @brief Finds one material by handle.
   inline const RenderMaterial *RenderScene::findMaterial(RenderMaterialHandle handle) const {
      const auto it = std::ranges::find(materials_, handle, &RenderMaterial::handle);
      return it == materials_.end() ? nullptr : std::addressof(*it);
   }

   /// @brief Finds one render instance by handle.
   inline const RenderInstance *RenderScene::findInstance(RenderInstanceHandle handle) const {
      const auto it = std::ranges::find(instances_, handle, &RenderInstance::handle);
      return it == instances_.end() ? nullptr : std::addressof(*it);
   }

   /// @brief Returns mesh count.
   inline std::size_t RenderScene::meshCount() const { return meshes_.size(); }

   /// @brief Returns material count.
   inline std::size_t RenderScene::materialCount() const { return materials_.size(); }

   /// @brief Returns instance count.
   inline std::size_t RenderScene::instanceCount() const { return instances_.size(); }

   /// @brief Returns total source vertex count.
   inline std::size_t RenderScene::vertexCount() const {
      std::size_t result{};
      for (const auto &mesh : meshes_) { result += mesh.vertices.size(); }
      return result;
   }

   /// @brief Returns total source index count.
   inline std::size_t RenderScene::indexCount() const {
      std::size_t result{};
      for (const auto &mesh : meshes_) { result += mesh.indices.size(); }
      return result;
   }

   /// @brief Returns the active render camera if one was set.
   inline const std::optional<RenderCamera> &RenderScene::camera() const { return camera_; }

   /// @brief Returns the active directional light if one was set.
   inline const std::optional<RenderDirectionalLight> &RenderScene::directionalLight() const { return light_; }

   /// @brief Returns all render instances.
   inline const Vector<RenderInstance> &RenderScene::instances() const { return instances_; }

   /// @brief Creates a renderer descriptor from a user-selected renderer id.
   inline std::expected<RendererDescriptor, Error> RenderSystem::createRenderer(RendererId id) const {
      const auto selector = std::string_view{id.value};
      const auto match = std::ranges::find(detail::renderer_choices, selector, &detail::RendererChoice::id);
      if (match != detail::renderer_choices.end()) { return createDescriptor(*match); }
      return std::unexpected(Error::invalid_argument);
   }

   /// @brief Creates the default forward-renderer descriptor.
   inline RendererDescriptor RenderSystem::createForwardRenderer() const {
      return createDescriptor(detail::renderer_choices.front());
   }

   /// @brief Builds and validates the render-pass DAG from a renderer's flat pass list.
   inline std::expected<RenderGraph, Error> RenderSystem::buildRenderGraph(const RendererDescriptor &renderer) const {
      return buildRenderGraph(renderer.passes);
   }

   /// @brief Builds and validates the render-pass DAG from flat pass lists supplied by engine systems.
   inline std::expected<RenderGraph, Error>
   RenderSystem::buildRenderGraph(std::span<const RenderPassContract> passes) const {
      const std::array lists{passes};
      return buildRenderGraph(lists);
   }

   /// @brief Builds and validates the render-pass DAG from flat pass lists supplied by engine systems.
   inline std::expected<RenderGraph, Error>
   RenderSystem::buildRenderGraph(std::span<const std::span<const RenderPassContract>> pass_lists) const {
      RenderGraph graph{};
      std::map<std::string_view, RenderPassHandle> pass_handles{};
      for (const auto passes : pass_lists) {
         for (const auto &pass : passes) {
            const auto added = addPass(graph, pass_handles, pass);
            if (!added) { return std::unexpected(added.error()); }
         }
      }
      for (const auto passes : pass_lists) {
         if (const auto linked = addDependencies(graph, pass_handles, passes); !linked) {
            return std::unexpected(linked.error());
         }
      }

      if (const auto order = graph.topologicalOrder(); !order) { return std::unexpected(order.error()); }
      return graph;
   }

   /// @brief Adds a renderable plane primitive to the active CPU scene.
   inline std::expected<void, Error> RenderSystem::addPlane(Vec2 half_extent, LinearColor color,
                                                            Transform transform) {
      const auto material = scene_.addMaterial(RenderMaterial{.base_color = color});
      const auto mesh = scene_.addPlaneMesh(half_extent);
      if (const auto instance = scene_.addInstance(mesh, material, transform); !instance) {
         return std::unexpected(instance.error());
      }
      return {};
   }

   /// @brief Adds a renderable cuboid primitive to the active CPU scene.
   inline std::expected<void, Error> RenderSystem::addCuboid(Vec3 minimum, Vec3 maximum, LinearColor color,
                                                             Transform transform) {
      const auto material = scene_.addMaterial(RenderMaterial{.base_color = color});
      const auto mesh = scene_.addCuboidMesh(minimum, maximum);
      if (const auto instance = scene_.addInstance(mesh, material, transform); !instance) {
         return std::unexpected(instance.error());
      }
      return {};
   }

   /// @brief Clears the active CPU render scene.
   inline void RenderSystem::clearScene() { scene_.clear(); }

   /// @brief Sets the active render camera in the CPU scene.
   inline void RenderSystem::setCamera(Camera camera, PixelExtent extent) {
      scene_.setCamera(RenderCamera{.camera = std::move(camera), .target_extent = extent});
   }

   /// @brief Sets the active directional light in the CPU scene.
   inline void RenderSystem::setDirectionalLight(Direction direction_to_light, LinearColor color,
                                                 LightIntensity intensity, LinearColor ambient) {
      scene_.setDirectionalLight(RenderDirectionalLight{.direction_to_light = direction_to_light,
                                                       .color = color,
                                                       .intensity = intensity,
                                                       .ambient = ambient});
   }

   /// @brief Returns mesh count in the active CPU scene.
   inline std::size_t RenderSystem::sceneMeshCount() const { return scene_.meshCount(); }

   /// @brief Returns material count in the active CPU scene.
   inline std::size_t RenderSystem::sceneMaterialCount() const { return scene_.materialCount(); }

   /// @brief Returns instance count in the active CPU scene.
   inline std::size_t RenderSystem::sceneInstanceCount() const { return scene_.instanceCount(); }

   /// @brief Returns total vertex count in the active CPU scene.
   inline std::size_t RenderSystem::sceneVertexCount() const { return scene_.vertexCount(); }

   /// @brief Returns total index count in the active CPU scene.
   inline std::size_t RenderSystem::sceneIndexCount() const { return scene_.indexCount(); }

   /// @brief Reports whether the active CPU scene has a camera.
   inline bool RenderSystem::hasSceneCamera() const { return scene_.camera().has_value(); }

   /// @brief Reports whether the active CPU scene has a directional light.
   inline bool RenderSystem::hasSceneDirectionalLight() const { return scene_.directionalLight().has_value(); }

   /// @brief Records that a frame reached the renderer; Vulkan execution is added in the next stage.
   inline std::expected<void, Error> RenderSystem::renderFrame(const WindowFrameData &windows) {
      last_rendered_window_count_ = std::ranges::count_if(windows.windows, [](const WindowInfo &window) {
         return !window.should_close;
      });
      ++rendered_frames_;
      return {};
   }

   /// @brief Prepares native Vulkan targets, then records the frame hook snapshot.
   inline std::expected<void, Error> RenderSystem::renderFrame(WindowSystem &windows) {
      if (const auto result = vulkan_.prepare(windows); !result) { return result; }
      if (vulkan_.ready()) {
         if (scene_.instanceCount() > 0) {
            if (const auto shader = ensureSceneShader(); !shader) { return shader; }
            if (const auto data = buildSceneDrawData(); !data) { return data; }
            if (const auto result = vulkan_.renderScene(detail::clear_color, scene_vertex_spirv_, "main",
                                                        scene_fragment_spirv_, "main", scene_vertices_,
                                                        scene_indices_, static_cast<std::uint32_t>(scene_.meshCount()),
                                                        static_cast<std::uint32_t>(scene_.instanceCount()),
                                                        scene_clip_from_world_); !result) {
               return result;
            }
         } else {
            if (const auto shader = ensureTriangleShader(); !shader) { return shader; }
            if (const auto result = vulkan_.renderTriangle(detail::clear_color, triangle_vertex_spirv_,
                                                           "main", triangle_fragment_spirv_, "main"); !result) {
               return result;
            }
         }
      }
      return renderFrame(WindowFrameData{.windows = windows.snapshot()});
   }

   /// @brief Returns how many frame hooks reached the render system.
   inline std::uint64_t RenderSystem::renderedFrameCount() const { return rendered_frames_; }

   /// @brief Returns how many Vulkan clear/present frame batches completed.
   inline std::uint64_t RenderSystem::presentedFrameCount() const { return vulkan_.presentedFrameCount(); }

   /// @brief Returns how many hardcoded triangle draw calls completed.
   inline std::uint64_t RenderSystem::triangleDrawCount() const { return vulkan_.triangleDrawCount(); }

   /// @brief Returns the hardcoded triangle vertex count.
   inline std::uint32_t RenderSystem::triangleVertexCount() const { return vulkan_.triangleVertexCount(); }

   /// @brief Returns how many scene buffer uploads completed.
   inline std::uint64_t RenderSystem::sceneUploadCount() const { return vulkan_.sceneUploadCount(); }

   /// @brief Returns how many source meshes were drawn by the uploaded scene path.
   inline std::uint64_t RenderSystem::sceneMeshDrawCount() const { return vulkan_.sceneMeshDrawCount(); }

   /// @brief Returns how many source instances were drawn by the uploaded scene path.
   inline std::uint64_t RenderSystem::sceneInstanceDrawCount() const { return vulkan_.sceneInstanceDrawCount(); }

   /// @brief Returns how many vertices were uploaded by the scene draw path.
   inline std::uint32_t RenderSystem::sceneDrawVertexCount() const { return vulkan_.sceneVertexCount(); }

   /// @brief Returns how many indices were uploaded by the scene draw path.
   inline std::uint32_t RenderSystem::sceneDrawIndexCount() const { return vulkan_.sceneIndexCount(); }

   /// @brief Returns how many CPU/GPU debug sample slots are expected for the scene draw.
   inline std::size_t RenderSystem::sceneDebugSampleCount() const {
      return static_cast<std::size_t>(std::ranges::count_if(scene_cpu_debug_, &RenderDebugSample::valid));
   }

   /// @brief Returns one CPU-computed debug sample.
   inline std::optional<RenderDebugSample> RenderSystem::sceneCpuDebugSample(std::size_t index) const {
      if (index >= scene_cpu_debug_.size() || !scene_cpu_debug_[index].valid) { return {}; }
      return scene_cpu_debug_[index];
   }

   /// @brief Returns one GPU-computed debug sample read back from Vulkan.
   inline std::optional<RenderDebugSample> RenderSystem::sceneGpuDebugSample(std::size_t index) const {
      const auto sample = vulkan_.sceneDebugSample(index);
      if (!sample) { return {}; }
      auto result = detail::gpuSample(*sample);
      return result.valid ? std::optional<RenderDebugSample>{result} : std::optional<RenderDebugSample>{};
   }

   /// @brief Returns the CPU/GPU clip-space mismatch for one sample.
   inline std::optional<float> RenderSystem::sceneDebugClipError(std::size_t index) const {
      const auto cpu = sceneCpuDebugSample(index);
      const auto gpu = sceneGpuDebugSample(index);
      if (!cpu || !gpu) { return {}; }
      return detail::clipError(*cpu, *gpu);
   }

   /// @brief Returns the CPU/GPU depth mismatch for one sample.
   inline std::optional<float> RenderSystem::sceneDebugDepthError(std::size_t index) const {
      const auto cpu = sceneCpuDebugSample(index);
      const auto gpu = sceneGpuDebugSample(index);
      if (!cpu || !gpu) { return {}; }
      return std::abs(cpu->depth - gpu->depth);
   }

   /// @brief Returns the last frame's non-closed window count.
   inline std::size_t RenderSystem::lastRenderedWindowCount() const { return last_rendered_window_count_; }

   /// @brief Returns how many visible native windows have prepared Vulkan frame targets.
   inline std::size_t RenderSystem::preparedGpuTargetCount() const { return vulkan_.targetCount(); }

   /// @brief Returns the fixed color used by the most recent Vulkan clear frame.
   inline std::array<float, 4> RenderSystem::lastClearColor() const { return vulkan_.lastClearColor(); }

   /// @brief Compiles and caches the built-in hardcoded triangle shader.
   inline std::expected<void, Error> RenderSystem::ensureTriangleShader() {
      if (triangle_shader_.has_value()) { return {}; }
      const auto source = std::filesystem::path{VVE_V4_SHADER_SOURCE_DIR} / "Triangle.slang";
      auto shader = shaders_.compileAndReflect(
         source, Vector<std::string>{"vveTriangleVertexMain", "vveTriangleFragmentMain"});
      if (!shader) { return std::unexpected(shader.error()); }

      auto vertex = shaders_.stageSpirv(*shader, ShaderStage::vertex);
      if (!vertex) { return std::unexpected(vertex.error()); }
      auto fragment = shaders_.stageSpirv(*shader, ShaderStage::fragment);
      if (!fragment) { return std::unexpected(fragment.error()); }

      triangle_shader_ = *shader;
      triangle_vertex_spirv_.assign(vertex->begin(), vertex->end());
      triangle_fragment_spirv_.assign(fragment->begin(), fragment->end());
      return {};
   }

   /// @brief Compiles and caches the built-in uploaded-scene unlit shader.
   inline std::expected<void, Error> RenderSystem::ensureSceneShader() {
      if (scene_shader_.has_value()) { return {}; }
      const auto source = std::filesystem::path{VVE_V4_SHADER_SOURCE_DIR} / "SceneUnlit.slang";
      auto shader = shaders_.compileAndReflect(
         source, Vector<std::string>{"vveSceneUnlitVertexMain", "vveSceneUnlitFragmentMain"});
      if (!shader) { return std::unexpected(shader.error()); }

      auto vertex = shaders_.stageSpirv(*shader, ShaderStage::vertex);
      if (!vertex) { return std::unexpected(vertex.error()); }
      auto fragment = shaders_.stageSpirv(*shader, ShaderStage::fragment);
      if (!fragment) { return std::unexpected(fragment.error()); }

      scene_shader_ = *shader;
      scene_vertex_spirv_.assign(vertex->begin(), vertex->end());
      scene_fragment_spirv_.assign(fragment->begin(), fragment->end());
      return {};
   }

   /// @brief Flattens the active CPU scene into one small uploaded-geometry draw packet.
   inline std::expected<void, Error> RenderSystem::buildSceneDrawData() {
      if (!scene_.camera()) { return std::unexpected(Error::missing_object); }
      scene_vertices_.clear();
      scene_indices_.clear();
      scene_cpu_debug_.fill(RenderDebugSample{});
      const auto clip_from_world = detail::clipFromWorld(*scene_.camera());
      scene_clip_from_world_ = detail::columns(clip_from_world);
      for (const auto &instance : scene_.instances()) {
         const auto *mesh = scene_.findMesh(instance.mesh);
         const auto *material = scene_.findMaterial(instance.material);
         if (mesh == nullptr || material == nullptr) { return std::unexpected(Error::missing_object); }

         const auto base = static_cast<std::uint32_t>(scene_vertices_.size());
         const auto color = material->base_color.value;
         for (const auto &vertex : mesh->vertices) {
            const auto world = math::add(vertex.position, instance.local_transform.translation.value);
            scene_vertices_.push_back(vh::SceneVertex{.x = static_cast<float>(world.x),
                                                      .y = static_cast<float>(world.y),
                                                      .z = static_cast<float>(world.z),
                                                      .r = static_cast<float>(color.x),
                                                      .g = static_cast<float>(color.y),
                                                      .b = static_cast<float>(color.z)});
         }
         for (const auto index : mesh->indices) { scene_indices_.push_back(base + index); }
      }
      if (scene_vertices_.empty() || scene_indices_.empty()) { return std::unexpected(Error::missing_object); }
      constexpr auto debug_vertices = std::array{0U, 6U};
      for (std::size_t slot{}; slot < debug_vertices.size(); ++slot) {
         const auto vertex_id = debug_vertices[slot];
         if (vertex_id < scene_vertices_.size()) {
            scene_cpu_debug_[slot] = detail::cpuSample(vertex_id, scene_vertices_[vertex_id], clip_from_world);
         }
      }
      return {};
   }

   /// @brief Adds one pass node and merges duplicate names so systems can share milestones.
   inline std::expected<void, Error>
   RenderSystem::addPass(RenderGraph &graph, std::map<std::string_view, RenderPassHandle> &handles,
                         const RenderPassContract &pass) {
      if (pass.name.empty()) { return std::unexpected(Error::invalid_argument); }
      if (handles.contains(pass.name)) { return {}; }
      auto handle = graph.addNode(ObjectName{.value = std::string(pass.name)});
      if (!handle) { return std::unexpected(handle.error()); }
      handles.emplace(pass.name, *handle);
      return {};
   }

   /// @brief Adds all dependency edges for one flat pass list.
   inline std::expected<void, Error>
   RenderSystem::addDependencies(RenderGraph &graph, const std::map<std::string_view, RenderPassHandle> &handles,
                                 std::span<const RenderPassContract> passes) {
      for (const auto &pass : passes) {
         const auto pass_handle = handles.at(pass.name);
         for (const auto dependency : pass.depends_on) {
            const auto dependency_handle = handles.find(dependency);
            if (dependency.empty()) { return std::unexpected(Error::invalid_argument); }
            if (dependency_handle == handles.end()) { return std::unexpected(Error::missing_object); }
            graph.addEdge(dependency_handle->second, pass_handle);
         }
      }
      return {};
   }

   /// @brief Creates a renderer descriptor from one registry row.
   inline RendererDescriptor RenderSystem::createDescriptor(detail::RendererChoice choice) {
      return RendererDescriptor{.handle = makeCounterHandle<RendererHandle>(),
                                .id = RendererId{.value = std::string(choice.id)},
                                .shadow_maps = choice.shadow_maps,
                                .passes = choice.passes};
   }

} // namespace vve::v4
