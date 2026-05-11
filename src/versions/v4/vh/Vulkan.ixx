module;
#include <cstdlib>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

export module VEEngine.V4:Vulkan;
import std;
import :VulkanLow;
import :Window;

/// @file
/// @brief Stateful Vulkan-Hpp helper objects built on top of the stateless vh layer.

export namespace vve::v4::vh {

   /// @brief Vulkan objects associated with one drawable window.
   struct FrameTarget {
      WindowHandle window{};                       ///< Engine window owning this target.
      PixelExtent extent{};                        ///< Current drawable extent.
      vk::raii::SurfaceKHR surface{nullptr};       ///< Platform presentation surface.
      vk::raii::SwapchainKHR swapchain{nullptr};   ///< Swapchain used by the renderer.
      vk::Format color_format{};                   ///< Swapchain image format.
      vk::Format depth_format{};                   ///< Depth image format.
      std::vector<vk::Image> images{};             ///< Swapchain-owned raw images.
      std::vector<vk::raii::ImageView> views{};    ///< Swapchain color views.
      vk::raii::DeviceMemory depth_memory{nullptr}; ///< Device-local depth memory.
      vk::raii::Image depth_image{nullptr};        ///< Depth attachment image.
      vk::raii::ImageView depth_view{nullptr};     ///< Depth attachment view.
      vk::ImageLayout depth_layout{};              ///< Current depth image layout.
      std::vector<vk::ImageLayout> layouts{};      ///< Current swapchain image layouts.
      vk::raii::PipelineLayout triangle_layout{nullptr}; ///< Layout for the smoke-test triangle.
      vk::raii::Pipeline triangle_pipeline{nullptr};      ///< Pipeline used to draw the smoke-test triangle.
      vk::raii::PipelineLayout scene_layout{nullptr};    ///< Layout for uploaded unlit scene geometry.
      vk::raii::Pipeline scene_pipeline{nullptr};        ///< Pipeline used to draw uploaded unlit scene geometry.
      vk::raii::PipelineLayout shadow_layout{nullptr};   ///< Pipeline layout for the first shadow-depth pass.
      vk::raii::Pipeline shadow_pipeline{nullptr};       ///< Depth-only pipeline for directional shadows.
      vk::raii::DeviceMemory scene_vertex_memory{nullptr}; ///< Memory backing the scene vertex buffer.
      vk::raii::Buffer scene_vertices{nullptr};          ///< Host-visible position/color vertex buffer.
      vk::raii::DeviceMemory scene_index_memory{nullptr}; ///< Memory backing the scene index buffer.
      vk::raii::Buffer scene_indices{nullptr};           ///< Host-visible index buffer.
      vk::raii::DeviceMemory shadow_depth_memory{nullptr}; ///< Memory backing the shadow depth image.
      vk::raii::Image shadow_depth_image{nullptr};       ///< Directional shadow depth image.
      vk::raii::ImageView shadow_depth_view{nullptr};    ///< Shadow depth image view.
      vk::raii::Sampler shadow_sampler{nullptr};         ///< Sampler used to verify shadow-map reads.
      vk::ImageLayout shadow_depth_layout{};              ///< Current shadow depth image layout.
      vk::raii::DeviceMemory shadow_readback_memory{nullptr}; ///< Memory backing the shadow readback buffer.
      vk::raii::Buffer shadow_readback{nullptr};          ///< Host-visible shadow depth readback buffer.
      vk::raii::DescriptorSetLayout scene_debug_layout{nullptr}; ///< Layout for shader debug readback.
      vk::raii::DescriptorPool scene_debug_pool{nullptr};         ///< Pool owning the debug descriptor set.
      vk::DescriptorSet scene_debug_set{};                       ///< Pool-owned descriptor set.
      vk::raii::DeviceMemory scene_debug_memory{nullptr};         ///< Memory backing the shader debug buffer.
      vk::raii::Buffer scene_debug_buffer{nullptr};               ///< Host-visible shader debug buffer.
      std::uint32_t scene_vertex_count{};       ///< Uploaded vertex count.
      std::uint32_t scene_index_count{};        ///< Uploaded index count.
   };

   /// @brief One uploaded debug-scene vertex: world position, normal, and RGB color.
   struct SceneVertex {
      float x{};  ///< World-space x position.
      float y{};  ///< World-space y position.
      float z{};  ///< World-space z position.
      float nx{}; ///< World-space normal x component.
      float ny{}; ///< World-space normal y component.
      float nz{}; ///< World-space normal z component.
      float r{};  ///< Linear red channel.
      float g{};  ///< Linear green channel.
      float b{};  ///< Linear blue channel.
   };

   inline constexpr std::uint32_t invalid_scene_debug_vertex =
      std::numeric_limits<std::uint32_t>::max(); ///< Sentinel used by empty debug slots.

   /// @brief Exact GPU-side vertex sample copied from the debug storage buffer.
   struct SceneDebugSample {
      std::uint32_t vertex_id{invalid_scene_debug_vertex}; ///< Vertex id written by the shader.
      std::array<float, 3> padding0{};                     ///< Matches shader 16-byte struct alignment.
      std::array<float, 3> world{};                        ///< GPU-observed world position.
      float padding1{};                                    ///< Matches shader 16-byte struct alignment.
      std::array<float, 4> clip{};                         ///< GPU-computed clip position.
      std::array<float, 4> light_clip{};                   ///< GPU-computed light clip position.
      std::array<float, 3> ndc{};                          ///< GPU-computed NDC position.
      float depth{};                                       ///< GPU-computed Vulkan depth.
      std::array<float, 3> light_ndc{};                    ///< GPU-computed light NDC position.
      float light_depth{};                                 ///< GPU-computed light depth.
      float sampled_shadow_depth{};                        ///< Shadow depth sampled by the shader.
      float shadow_depth_delta{};                          ///< Light depth minus sampled shadow depth.
      float shadow_bias{};                                  ///< Bias used by the shadow comparison.
      float shadow_factor{};                                ///< One when lit, zero when shadowed.
      std::array<float, 3> normal{};                       ///< GPU-normalized surface normal.
      float n_dot_l{};                                     ///< GPU-computed Lambert term.
      std::array<float, 3> direction_to_light{};           ///< GPU-normalized direction to light.
      float intensity{};                                   ///< Direct-light intensity.
      std::array<float, 3> ambient_lighting{};             ///< Ambient light contribution.
      float direct_factor{};                               ///< Direct-light scalar factor.
      std::array<float, 3> direct_lighting{};              ///< Direct light contribution.
      float unused0{};                                     ///< Layout padding visible to the host.
      std::array<float, 3> point_lighting{};               ///< Point light contribution.
      float unused1{};                                     ///< Layout padding visible to the host.
      std::array<float, 3> spot_lighting{};                ///< Spot light contribution.
      float unused2{};                                     ///< Layout padding visible to the host.
      std::array<float, 3> final_lighting{};               ///< Ambient plus direct lighting.
      float unused3{};                                     ///< Layout padding visible to the host.
   };
   static_assert(sizeof(SceneDebugSample) == 224);

   /// @brief Owns Vulkan instance, device, and frame targets for visible windows.
   class FrameHost {
   public:
      FrameHost() = default;
      ~FrameHost();
      FrameHost(FrameHost &&other) noexcept;
      FrameHost &operator=(FrameHost &&other) noexcept;
      FrameHost(const FrameHost &) = delete;
      FrameHost &operator=(const FrameHost &) = delete;

      [[nodiscard]] std::expected<void, Error> prepare(WindowSystem &windows);
      [[nodiscard]] std::expected<void, Error> renderClear(const std::array<float, 4> &color);
      [[nodiscard]] std::expected<void, Error>
      renderTriangle(const std::array<float, 4> &color, std::span<const std::uint32_t> vertex_spirv,
                     std::string_view vertex_entry, std::span<const std::uint32_t> fragment_spirv,
                     std::string_view fragment_entry);
      [[nodiscard]] std::expected<void, Error>
      renderScene(const std::array<float, 4> &color, std::span<const std::uint32_t> vertex_spirv,
                  std::string_view vertex_entry, std::span<const std::uint32_t> fragment_spirv,
                  std::string_view fragment_entry, std::span<const std::uint32_t> shadow_vertex_spirv,
                  std::string_view shadow_vertex_entry, std::span<const SceneVertex> vertices,
                  std::span<const std::uint32_t> indices, std::uint32_t mesh_count,
                  std::uint32_t instance_count, std::span<const float> scene_constants);
      [[nodiscard]] std::size_t targetCount() const;
      [[nodiscard]] bool ready() const;
      [[nodiscard]] std::uint64_t presentedFrameCount() const;
      [[nodiscard]] std::uint64_t triangleDrawCount() const;
      [[nodiscard]] std::uint32_t triangleVertexCount() const;
      [[nodiscard]] std::uint64_t sceneUploadCount() const;
      [[nodiscard]] std::uint64_t sceneMeshDrawCount() const;
      [[nodiscard]] std::uint64_t sceneInstanceDrawCount() const;
      [[nodiscard]] std::uint32_t sceneVertexCount() const;
      [[nodiscard]] std::uint32_t sceneIndexCount() const;
      [[nodiscard]] std::size_t sceneDebugSampleCount() const;
      [[nodiscard]] std::optional<SceneDebugSample> sceneDebugSample(std::size_t index) const;
      [[nodiscard]] PixelExtent sceneShadowExtent() const;
      [[nodiscard]] std::optional<float> sceneShadowDepth(std::uint32_t x, std::uint32_t y) const;
      [[nodiscard]] std::array<float, 4> lastClearColor() const;

   private:
      [[nodiscard]] std::expected<void, Error> createInstance();
      [[nodiscard]] std::expected<void, Error> rebuildTargets(std::span<std::reference_wrapper<Window>> windows);
      [[nodiscard]] std::expected<void, Error> createSurface(FrameTarget &target, Window &window);
      [[nodiscard]] std::expected<void, Error> createDevice(std::span<const vk::SurfaceKHR> surfaces);
      [[nodiscard]] std::expected<void, Error> createSwapchain(FrameTarget &target, Window &window);
      [[nodiscard]] std::expected<void, Error> createDepth(FrameTarget &target);
      [[nodiscard]] std::expected<void, Error> createFrameExecutor();
      [[nodiscard]] std::expected<void, Error> clearTarget(FrameTarget &target, const vk::ClearColorValue &color);
      [[nodiscard]] std::expected<void, Error>
      createTrianglePipeline(FrameTarget &target, std::span<const std::uint32_t> vertex_spirv,
                             std::string_view vertex_entry, std::span<const std::uint32_t> fragment_spirv,
                             std::string_view fragment_entry);
      [[nodiscard]] std::expected<void, Error>
      createScenePipeline(FrameTarget &target, std::span<const std::uint32_t> vertex_spirv,
                          std::string_view vertex_entry, std::span<const std::uint32_t> fragment_spirv,
                          std::string_view fragment_entry);
      [[nodiscard]] std::expected<void, Error>
      createSceneShadowPipeline(FrameTarget &target, std::span<const std::uint32_t> vertex_spirv,
                                std::string_view vertex_entry);
      [[nodiscard]] std::expected<void, Error>
      uploadScene(FrameTarget &target, std::span<const SceneVertex> vertices,
                  std::span<const std::uint32_t> indices);
      [[nodiscard]] std::expected<void, Error> createSceneShadowTarget(FrameTarget &target);
      [[nodiscard]] std::expected<void, Error> createSceneDebugTarget(FrameTarget &target);
      [[nodiscard]] std::expected<void, Error> clearSceneDebugTarget(FrameTarget &target);
      [[nodiscard]] std::expected<void, Error> readSceneDebugTarget(FrameTarget &target);
      template <typename TRecord> [[nodiscard]] std::expected<void, Error>
      presentFrame(FrameTarget &target, TRecord &&record);
      [[nodiscard]] std::expected<void, Error> drawTriangleTarget(FrameTarget &target,
                                                                  const vk::ClearColorValue &color);
      [[nodiscard]] std::expected<void, Error> drawSceneTarget(FrameTarget &target,
                                                               const vk::ClearColorValue &color,
                                                               std::span<const float> scene_constants);
      [[nodiscard]] std::expected<void, Error> drawSceneShadowTarget(FrameTarget &target,
                                                                     std::span<const float> scene_constants);
      [[nodiscard]] bool matches(std::span<const std::reference_wrapper<Window>> windows) const;
      void clearFrameExecutor();
      void clearTargets();
      void reset();

      vk::raii::Context context_{};              ///< Vulkan-Hpp dispatcher root for RAII handles.
      vk::raii::Instance instance_{nullptr};     ///< RAII Vulkan instance.
      vk::raii::PhysicalDevice physical_device_{nullptr}; ///< Selected physical device wrapper.
      vk::raii::Device device_{nullptr};         ///< RAII logical device.
      vk::raii::Queue queue_{nullptr};           ///< Non-owning RAII graphics/present queue.
      std::uint32_t queue_family_{};
      vk::raii::CommandPool command_pool_{nullptr}; ///< RAII command-pool owner.
      vk::raii::CommandBuffer command_buffer_{nullptr}; ///< RAII command buffer.
      vk::raii::Semaphore frame_timeline_{nullptr}; ///< Timeline semaphore for serial submissions.
      vk::raii::Fence acquire_fence_{nullptr};  ///< Fence used for swapchain acquisition.
      std::vector<FrameTarget> targets_{};
      std::uint64_t timeline_value_{0};
      std::uint64_t presented_frames_{0};
      std::uint64_t triangle_draws_{0};
      std::uint64_t scene_uploads_{0};
      std::uint64_t scene_mesh_draws_{0};
      std::uint64_t scene_instance_draws_{0};
      std::uint32_t scene_vertex_count_{0};
      std::uint32_t scene_index_count_{0};
      std::array<SceneDebugSample, 2> scene_debug_samples_{}; ///< Last shader debug readback samples.
      std::vector<float> scene_shadow_depths_{}; ///< Last shadow-depth image copied back to the host.
      std::array<float, 4> last_clear_color_{};
   };

} // namespace vve::v4::vh

namespace vve::v4::vh::detail {

   /// @brief Reads one process environment variable.
   [[nodiscard]] std::string environmentValue(const char *name) {
      const char *value = std::getenv(name);
      return value == nullptr ? std::string{} : std::string{value};
   }

   /// @brief Returns the CMake-selected ICD when the user did not override it.
   [[nodiscard]] std::string defaultVulkanIcd() {
#ifdef VVE_DEFAULT_VULKAN_ICD
      return std::string{VVE_DEFAULT_VULKAN_ICD};
#else
      return {};
#endif
   }

   /// @brief Returns the generated ICD manifest for a known driver selector.
   [[nodiscard]] std::optional<std::filesystem::path> icdManifest(std::string_view selector) {
      if (selector == "moltenvk") {
#ifdef VVE_MOLTENVK_ICD_MANIFEST
         return std::filesystem::path{VVE_MOLTENVK_ICD_MANIFEST};
#else
         return {};
#endif
      }
      if (selector == "kosmickrisp") {
#ifdef VVE_KOSMICKRISP_ICD_MANIFEST
         return std::filesystem::path{VVE_KOSMICKRISP_ICD_MANIFEST};
#else
         return {};
#endif
      }
      return {};
   }

   /// @brief Applies the default ICD only when the user did not already choose one.
   void configureVulkanIcd() {
      if (!environmentValue("VK_ICD_FILENAMES").empty()) { return; }
      auto selector = environmentValue("VVE_VULKAN_ICD");
      if (selector.empty()) { selector = defaultVulkanIcd(); }
      if (selector.empty() || selector == "system") { return; }

      const auto manifest = icdManifest(selector);
      if (!manifest || !std::filesystem::is_regular_file(*manifest)) { return; }
      const auto value = manifest->string();
#if defined(_WIN32)
      _putenv_s("VK_ICD_FILENAMES", value.c_str());
#else
      setenv("VK_ICD_FILENAMES", value.c_str(), 1);
#endif
   }

   /// @brief Checks whether a Vulkan-Hpp RAII wrapper currently contains a live raw handle.
   template <typename THandle> [[nodiscard]] bool has(const THandle &handle) {
      return static_cast<bool>(*handle);
   }

} // namespace vve::v4::vh::detail

namespace vve::v4::vh {

   /// @brief Releases all owned Vulkan objects.
   FrameHost::~FrameHost() = default;

   /// @brief Moves ownership of all Vulkan objects.
   FrameHost::FrameHost(FrameHost &&other) noexcept = default;

   /// @brief Moves ownership of all Vulkan objects.
   FrameHost &FrameHost::operator=(FrameHost &&other) noexcept = default;

   /// @brief Ensures every visible native SDL window has a swapchain and depth image.
   std::expected<void, Error> FrameHost::prepare(WindowSystem &windows) {
      auto refs = windows.windows();
      auto native = std::vector<std::reference_wrapper<Window>>{};
      for (Window &window : refs | std::views::transform([](auto ref) -> Window & { return ref.get(); })) {
         if (window.native() != nullptr && !window.shouldClose() && !window.minimized()) { native.push_back(window); }
      }
      if (native.empty()) { clearTargets(); return {}; }
      if (detail::has(device_) && matches(native)) { return {}; }
      return rebuildTargets(native);
   }

   /// @brief Clears and presents all prepared targets once using dynamic rendering.
   std::expected<void, Error> FrameHost::renderClear(const std::array<float, 4> &color) {
      if (!ready()) { return {}; }
      const auto clear = vk::ClearColorValue{color};
      for (auto &target : targets_) {
         if (const auto result = clearTarget(target, clear); !result) { return result; }
      }
      last_clear_color_ = color;
      ++presented_frames_;
      return {};
   }

   /// @brief Clears, draws one hardcoded triangle, and presents every prepared target.
   std::expected<void, Error>
   FrameHost::renderTriangle(const std::array<float, 4> &color, std::span<const std::uint32_t> vertex_spirv,
                             std::string_view vertex_entry, std::span<const std::uint32_t> fragment_spirv,
                             std::string_view fragment_entry) {
      if (!ready()) { return {}; }
      const auto clear = vk::ClearColorValue{color};
      for (auto &target : targets_) {
         if (const auto result = createTrianglePipeline(target, vertex_spirv, vertex_entry,
                                                        fragment_spirv, fragment_entry); !result) {
            return result;
         }
         if (const auto result = drawTriangleTarget(target, clear); !result) { return result; }
      }
      last_clear_color_ = color;
      triangle_draws_ += targets_.size();
      ++presented_frames_;
      return {};
   }

   /// @brief Uploads, clears, draws the debug scene, and presents every prepared target.
   std::expected<void, Error>
   FrameHost::renderScene(const std::array<float, 4> &color, std::span<const std::uint32_t> vertex_spirv,
                          std::string_view vertex_entry, std::span<const std::uint32_t> fragment_spirv,
                          std::string_view fragment_entry, std::span<const std::uint32_t> shadow_vertex_spirv,
                          std::string_view shadow_vertex_entry, std::span<const SceneVertex> vertices,
                          std::span<const std::uint32_t> indices, std::uint32_t mesh_count,
                          std::uint32_t instance_count, std::span<const float> scene_constants) {
      if (!ready()) { return {}; }
      if (vertices.empty() || indices.empty() || scene_constants.size() < 52) {
         return std::unexpected(Error::invalid_argument);
      }

      const auto clear = vk::ClearColorValue{color};
      for (auto &target : targets_) {
         if (const auto result = createSceneShadowTarget(target); !result) { return result; }
         if (const auto result = createSceneDebugTarget(target); !result) { return result; }
         if (const auto result = createScenePipeline(target, vertex_spirv, vertex_entry,
                                                     fragment_spirv, fragment_entry); !result) {
            return result;
         }
         if (const auto result = createSceneShadowPipeline(target, shadow_vertex_spirv,
                                                          shadow_vertex_entry); !result) {
            return result;
         }
         if (const auto result = uploadScene(target, vertices, indices); !result) { return result; }
         if (const auto result = clearSceneDebugTarget(target); !result) { return result; }
         if (const auto result = drawSceneShadowTarget(target, scene_constants); !result) { return result; }
         if (const auto result = drawSceneTarget(target, clear, scene_constants); !result) { return result; }
         if (const auto result = readSceneDebugTarget(target); !result) { return result; }
      }
      last_clear_color_ = color;
      scene_uploads_ += targets_.size();
      scene_mesh_draws_ += static_cast<std::uint64_t>(mesh_count) * targets_.size();
      scene_instance_draws_ += static_cast<std::uint64_t>(instance_count) * targets_.size();
      scene_vertex_count_ = static_cast<std::uint32_t>(vertices.size());
      scene_index_count_ = static_cast<std::uint32_t>(indices.size());
      ++presented_frames_;
      return {};
   }

   /// @brief Returns how many frame targets are currently prepared.
   std::size_t FrameHost::targetCount() const { return targets_.size(); }

   /// @brief Reports whether at least one drawable Vulkan target exists.
   bool FrameHost::ready() const {
      return detail::has(instance_) && detail::has(device_) && !targets_.empty();
   }

   /// @brief Returns how many clear/present frame batches completed.
   std::uint64_t FrameHost::presentedFrameCount() const { return presented_frames_; }

   /// @brief Returns how many hardcoded triangle draw calls completed.
   std::uint64_t FrameHost::triangleDrawCount() const { return triangle_draws_; }

   /// @brief Returns the hardcoded triangle vertex count.
   std::uint32_t FrameHost::triangleVertexCount() const { return 3; }

   /// @brief Returns how many scene buffer uploads completed.
   std::uint64_t FrameHost::sceneUploadCount() const { return scene_uploads_; }

   /// @brief Returns how many mesh draws were represented by the last scene draw path.
   std::uint64_t FrameHost::sceneMeshDrawCount() const { return scene_mesh_draws_; }

   /// @brief Returns how many instance draws were represented by the last scene draw path.
   std::uint64_t FrameHost::sceneInstanceDrawCount() const { return scene_instance_draws_; }

   /// @brief Returns the last uploaded scene vertex count.
   std::uint32_t FrameHost::sceneVertexCount() const { return scene_vertex_count_; }

   /// @brief Returns the last uploaded scene index count.
   std::uint32_t FrameHost::sceneIndexCount() const { return scene_index_count_; }

   /// @brief Returns how many GPU scene-debug sample slots contain data.
   std::size_t FrameHost::sceneDebugSampleCount() const {
      return static_cast<std::size_t>(std::ranges::count_if(scene_debug_samples_, [](const auto &sample) {
         return sample.vertex_id != invalid_scene_debug_vertex;
      }));
   }

   /// @brief Returns one GPU scene-debug sample if the shader wrote it.
   std::optional<SceneDebugSample> FrameHost::sceneDebugSample(std::size_t index) const {
      if (index >= scene_debug_samples_.size()) { return {}; }
      const auto sample = scene_debug_samples_[index];
      return sample.vertex_id == invalid_scene_debug_vertex ? std::optional<SceneDebugSample>{} : sample;
   }

   /// @brief Returns the fixed shadow-map extent used by the first proof pass.
   PixelExtent FrameHost::sceneShadowExtent() const { return PixelExtent{.width = 1024, .height = 1024}; }

   /// @brief Returns one copied shadow depth value if the latest pass produced it.
   std::optional<float> FrameHost::sceneShadowDepth(std::uint32_t x, std::uint32_t y) const {
      const auto extent = sceneShadowExtent();
      if (x >= extent.width || y >= extent.height) { return {}; }
      const auto index = static_cast<std::size_t>(y) * extent.width + x;
      return index < scene_shadow_depths_.size() ? std::optional<float>{scene_shadow_depths_[index]}
                                                 : std::optional<float>{};
   }

   /// @brief Returns the fixed clear color used by the most recent clear frame.
   std::array<float, 4> FrameHost::lastClearColor() const { return last_clear_color_; }

   /// @brief Creates the Vulkan instance using SDL's required platform extensions.
   std::expected<void, Error> FrameHost::createInstance() {
      detail::configureVulkanIcd();
      std::uint32_t sdl_count{};
      const char *const *sdl_extensions = SDL_Vulkan_GetInstanceExtensions(&sdl_count);
      if (sdl_extensions == nullptr) { return std::unexpected(Error::platform_error); }
      const auto extensions = std::span<const char *const>{sdl_extensions, sdl_count};
      auto raw_instance = vk::Instance{};
      const auto result = low::createInstance("ViennaVulkanEngine v4", extensions, &raw_instance);
      if (result == vk::Result::eSuccess) {
         instance_ = vk::raii::Instance{context_, static_cast<VkInstance>(raw_instance)};
      }
      return result == vk::Result::eSuccess ? std::expected<void, Error>{} : std::unexpected(Error::platform_error);
   }

   /// @brief Rebuilds surfaces, swapchains, and depth targets for the current native windows.
   std::expected<void, Error> FrameHost::rebuildTargets(std::span<std::reference_wrapper<Window>> windows) {
      if (!detail::has(instance_)) {
         if (const auto result = createInstance(); !result) { return result; }
      }
      clearTargets();

      auto surfaces = std::vector<vk::SurfaceKHR>{};
      for (Window &window : windows | std::views::transform([](auto ref) -> Window & { return ref.get(); })) {
         auto &target = targets_.emplace_back(FrameTarget{.window = window.handle(), .extent = window.extent()});
         if (const auto result = createSurface(target, window); !result) { reset(); return result; }
         surfaces.push_back(*target.surface);
      }

      if (!detail::has(device_)) {
         if (const auto result = createDevice(surfaces); !result) { reset(); return result; }
      }
      for (std::size_t i{}; i < targets_.size(); ++i) {
         if (const auto result = createSwapchain(targets_[i], windows[i].get()); !result) { reset(); return result; }
         if (const auto result = createDepth(targets_[i]); !result) { reset(); return result; }
      }
      return {};
   }

   /// @brief Creates one presentation surface for one native SDL window.
   std::expected<void, Error> FrameHost::createSurface(FrameTarget &target, Window &window) {
      VkSurfaceKHR raw_surface{};
      const auto raw_instance = static_cast<VkInstance>(*instance_);
      if (!SDL_Vulkan_CreateSurface(window.native(), raw_instance, nullptr, &raw_surface)) {
         return std::unexpected(Error::platform_error);
      }
      target.surface = vk::raii::SurfaceKHR{instance_, raw_surface};
      if (!detail::has(device_)) { return {}; }

      vk::Bool32 supported{};
      const auto result = (*physical_device_).getSurfaceSupportKHR(queue_family_, *target.surface, &supported);
      if (result != vk::Result::eSuccess || supported != VK_TRUE) { return std::unexpected(Error::platform_error); }
      return {};
   }

   /// @brief Creates the logical Vulkan device selected by the stateless helper layer.
   std::expected<void, Error> FrameHost::createDevice(std::span<const vk::SurfaceKHR> surfaces) {
      auto extensions = std::vector<std::string>{};
      auto raw_physical_device = vk::PhysicalDevice{};
      if (!low::chooseDevice(*instance_, surfaces, &raw_physical_device, &queue_family_, &extensions)) {
         return std::unexpected(Error::platform_error);
      }
      physical_device_ = vk::raii::PhysicalDevice{instance_, static_cast<VkPhysicalDevice>(raw_physical_device)};

      auto raw_device = vk::Device{};
      auto raw_queue = vk::Queue{};
      const auto result = low::createDevice(*physical_device_, queue_family_, extensions, &raw_device, &raw_queue);
      if (result == vk::Result::eSuccess) {
         device_ = vk::raii::Device{physical_device_, static_cast<VkDevice>(raw_device)};
         queue_ = vk::raii::Queue{device_, static_cast<VkQueue>(raw_queue)};
      }
      if (result != vk::Result::eSuccess) { return std::unexpected(Error::platform_error); }
      return createFrameExecutor();
   }

   /// @brief Creates swapchain images and color views for one frame target.
   std::expected<void, Error> FrameHost::createSwapchain(FrameTarget &target, Window &window) {
      const auto fallback = vk::Extent2D{window.extent().width, window.extent().height};
      auto raw_swapchain = vk::SwapchainKHR{};
      auto raw_views = std::vector<vk::ImageView>{};
      auto extent = vk::Extent2D{};
      const auto result = low::createSwapchainAndViews(*physical_device_, *device_, *target.surface, fallback,
                                                       &raw_swapchain, &target.color_format, &extent,
                                                       &target.images, &raw_views);
      if (result == vk::Result::eSuccess) {
         target.swapchain = vk::raii::SwapchainKHR{device_, static_cast<VkSwapchainKHR>(raw_swapchain)};
         target.views.clear();
         target.views.reserve(raw_views.size());
         for (const auto view : raw_views) {
            target.views.emplace_back(device_, static_cast<VkImageView>(view));
         }
      }
      target.extent = PixelExtent{.width = extent.width, .height = extent.height};
      target.layouts.assign(target.images.size(), vk::ImageLayout::eUndefined);
      return result == vk::Result::eSuccess ? std::expected<void, Error>{} : std::unexpected(Error::platform_error);
   }

   /// @brief Creates a depth attachment for one frame target.
   std::expected<void, Error> FrameHost::createDepth(FrameTarget &target) {
      const auto extent = vk::Extent2D{target.extent.width, target.extent.height};
      auto raw_image = vk::Image{};
      auto raw_memory = vk::DeviceMemory{};
      auto raw_view = vk::ImageView{};
      const auto result = low::createDepthTarget(*physical_device_, *device_, extent, &target.depth_format,
                                                 &raw_image, &raw_memory, &raw_view);
      if (result == vk::Result::eSuccess) {
         target.depth_memory = vk::raii::DeviceMemory{device_, static_cast<VkDeviceMemory>(raw_memory)};
         target.depth_image = vk::raii::Image{device_, static_cast<VkImage>(raw_image)};
         target.depth_view = vk::raii::ImageView{device_, static_cast<VkImageView>(raw_view)};
      }
      target.depth_layout = vk::ImageLayout::eUndefined;
      return result == vk::Result::eSuccess ? std::expected<void, Error>{} : std::unexpected(Error::platform_error);
   }

   /// @brief Creates reusable frame command and timeline synchronization objects.
   std::expected<void, Error> FrameHost::createFrameExecutor() {
      auto raw_pool = vk::CommandPool{};
      auto raw_buffer = vk::CommandBuffer{};
      auto raw_timeline = vk::Semaphore{};
      auto raw_fence = vk::Fence{};
      const auto result = low::createFrameExecutor(*device_, queue_family_, &raw_pool, &raw_buffer,
                                                   &raw_timeline, &raw_fence);
      if (result == vk::Result::eSuccess) {
         command_pool_ = vk::raii::CommandPool{device_, static_cast<VkCommandPool>(raw_pool)};
         command_buffer_ = vk::raii::CommandBuffer{device_, static_cast<VkCommandBuffer>(raw_buffer), raw_pool};
         frame_timeline_ = vk::raii::Semaphore{device_, static_cast<VkSemaphore>(raw_timeline)};
         acquire_fence_ = vk::raii::Fence{device_, static_cast<VkFence>(raw_fence)};
      }
      return result == vk::Result::eSuccess ? std::expected<void, Error>{} : std::unexpected(Error::platform_error);
   }

   /// @brief Creates the smoke-test triangle pipeline for one target if needed.
   std::expected<void, Error>
   FrameHost::createTrianglePipeline(FrameTarget &target, std::span<const std::uint32_t> vertex_spirv,
                                     std::string_view vertex_entry,
                                     std::span<const std::uint32_t> fragment_spirv,
                                     std::string_view fragment_entry) {
      if (detail::has(target.triangle_pipeline)) { return {}; }
      const auto color_formats = std::array{target.color_format};
      auto raw_layout = vk::PipelineLayout{};
      auto raw_pipeline = vk::Pipeline{};
      const auto result = low::createGraphicsPipeline(*device_, vertex_spirv, vertex_entry, fragment_spirv,
                                                      fragment_entry, {}, {}, {}, {}, color_formats, {},
                                                      false, &raw_layout, &raw_pipeline);
      if (result == vk::Result::eSuccess) {
         target.triangle_layout = vk::raii::PipelineLayout{device_, static_cast<VkPipelineLayout>(raw_layout)};
         target.triangle_pipeline = vk::raii::Pipeline{device_, static_cast<VkPipeline>(raw_pipeline)};
      }
      return result == vk::Result::eSuccess ? std::expected<void, Error>{} : std::unexpected(Error::platform_error);
   }

   /// @brief Creates the uploaded-scene pipeline for one target if needed.
   std::expected<void, Error>
   FrameHost::createScenePipeline(FrameTarget &target, std::span<const std::uint32_t> vertex_spirv,
                                  std::string_view vertex_entry,
                                  std::span<const std::uint32_t> fragment_spirv,
                                  std::string_view fragment_entry) {
      if (detail::has(target.scene_pipeline)) { return {}; }
      const auto push_range = vk::PushConstantRange{vk::ShaderStageFlagBits::eVertex |
                                                    vk::ShaderStageFlagBits::eFragment, 0,
                                                    52U * static_cast<std::uint32_t>(sizeof(float))};
      const auto set_layouts = std::array{*target.scene_debug_layout};
      const auto push_ranges = std::array{push_range};
      const auto color_formats = std::array{target.color_format};
      const auto stride = 9U * static_cast<std::uint32_t>(sizeof(float));
      const auto vertex_binding = vk::VertexInputBindingDescription{0, stride, vk::VertexInputRate::eVertex};
      const auto bindings = std::array{vertex_binding};
      const auto attributes = std::array{vk::VertexInputAttributeDescription{0, 0, vk::Format::eR32G32B32Sfloat, 0},
                                         vk::VertexInputAttributeDescription{1, 0, vk::Format::eR32G32B32Sfloat,
                                                                            3U * static_cast<std::uint32_t>(
                                                                                    sizeof(float))},
                                         vk::VertexInputAttributeDescription{2, 0, vk::Format::eR32G32B32Sfloat,
                                                                            6U * static_cast<std::uint32_t>(
                                                                                    sizeof(float))}};
      auto raw_layout = vk::PipelineLayout{};
      auto raw_pipeline = vk::Pipeline{};
      const auto result = low::createGraphicsPipeline(*device_, vertex_spirv, vertex_entry, fragment_spirv,
                                                      fragment_entry, set_layouts, push_ranges, bindings,
                                                      attributes, color_formats, target.depth_format, true,
                                                      &raw_layout, &raw_pipeline);
      if (result == vk::Result::eSuccess) {
         target.scene_layout = vk::raii::PipelineLayout{device_, static_cast<VkPipelineLayout>(raw_layout)};
         target.scene_pipeline = vk::raii::Pipeline{device_, static_cast<VkPipeline>(raw_pipeline)};
      }
      return result == vk::Result::eSuccess ? std::expected<void, Error>{} : std::unexpected(Error::platform_error);
   }

   /// @brief Creates the shadow-depth pipeline for one target if needed.
   std::expected<void, Error>
   FrameHost::createSceneShadowPipeline(FrameTarget &target, std::span<const std::uint32_t> vertex_spirv,
                                        std::string_view vertex_entry) {
      if (detail::has(target.shadow_pipeline)) { return {}; }
      const auto push_range = vk::PushConstantRange{vk::ShaderStageFlagBits::eVertex, 0,
                                                    52U * static_cast<std::uint32_t>(sizeof(float))};
      const auto push_ranges = std::array{push_range};
      const auto stride = 9U * static_cast<std::uint32_t>(sizeof(float));
      const auto vertex_binding = vk::VertexInputBindingDescription{0, stride, vk::VertexInputRate::eVertex};
      const auto bindings = std::array{vertex_binding};
      const auto attributes = std::array{vk::VertexInputAttributeDescription{0, 0,
                                                                            vk::Format::eR32G32B32Sfloat, 0}};
      auto raw_layout = vk::PipelineLayout{};
      auto raw_pipeline = vk::Pipeline{};
      const auto result = low::createGraphicsPipeline(*device_, vertex_spirv, vertex_entry, {}, {}, {}, push_ranges,
                                                      bindings, attributes, {}, vk::Format::eD32Sfloat, true,
                                                      &raw_layout, &raw_pipeline);
      if (result == vk::Result::eSuccess) {
         target.shadow_layout = vk::raii::PipelineLayout{device_, static_cast<VkPipelineLayout>(raw_layout)};
         target.shadow_pipeline = vk::raii::Pipeline{device_, static_cast<VkPipeline>(raw_pipeline)};
      }
      return result == vk::Result::eSuccess ? std::expected<void, Error>{} : std::unexpected(Error::platform_error);
   }

   /// @brief Uploads scene vertex and index data directly into host-visible buffers for the proof renderer.
   std::expected<void, Error>
   FrameHost::uploadScene(FrameTarget &target, std::span<const SceneVertex> vertices,
                          std::span<const std::uint32_t> indices) {
      const auto vertex_bytes = std::as_bytes(vertices);
      const auto index_bytes = std::as_bytes(indices);
      const auto same_size = target.scene_vertex_count == vertices.size() &&
                             target.scene_index_count == indices.size();
      if (!detail::has(target.scene_vertices) || !detail::has(target.scene_indices) || !same_size) {
         target.scene_vertices.clear();
         target.scene_vertex_memory.clear();
         target.scene_indices.clear();
         target.scene_index_memory.clear();

         auto raw_buffer = vk::Buffer{};
         auto raw_memory = vk::DeviceMemory{};
         auto created = low::createHostBuffer(*physical_device_, *device_, vertex_bytes.size(),
                                              vk::BufferUsageFlagBits::eVertexBuffer,
                                              &raw_buffer, &raw_memory);
         if (created != vk::Result::eSuccess) { return std::unexpected(Error::platform_error); }
         target.scene_vertex_memory = vk::raii::DeviceMemory{device_, static_cast<VkDeviceMemory>(raw_memory)};
         target.scene_vertices = vk::raii::Buffer{device_, static_cast<VkBuffer>(raw_buffer)};

         raw_buffer = nullptr;
         raw_memory = nullptr;
         created = low::createHostBuffer(*physical_device_, *device_, index_bytes.size(),
                                         vk::BufferUsageFlagBits::eIndexBuffer,
                                         &raw_buffer, &raw_memory);
         if (created != vk::Result::eSuccess) { return std::unexpected(Error::platform_error); }
         target.scene_index_memory = vk::raii::DeviceMemory{device_, static_cast<VkDeviceMemory>(raw_memory)};
         target.scene_indices = vk::raii::Buffer{device_, static_cast<VkBuffer>(raw_buffer)};
      }

      auto result = low::writeBuffer(*device_, *target.scene_vertex_memory, vertex_bytes);
      if (result != vk::Result::eSuccess) { return std::unexpected(Error::platform_error); }
      result = low::writeBuffer(*device_, *target.scene_index_memory, index_bytes);
      if (result != vk::Result::eSuccess) { return std::unexpected(Error::platform_error); }
      target.scene_vertex_count = static_cast<std::uint32_t>(vertices.size());
      target.scene_index_count = static_cast<std::uint32_t>(indices.size());
      return {};
   }

   /// @brief Creates the shadow depth image and host readback buffer.
   std::expected<void, Error> FrameHost::createSceneShadowTarget(FrameTarget &target) {
      if (detail::has(target.shadow_depth_image) && detail::has(target.shadow_readback) &&
          detail::has(target.shadow_sampler)) {
         return {};
      }
      const auto extent = sceneShadowExtent();
      const auto vk_extent = vk::Extent2D{extent.width, extent.height};
      auto raw_image = vk::Image{};
      auto raw_memory = vk::DeviceMemory{};
      auto raw_view = vk::ImageView{};
      auto result = low::createShadowDepthTarget(*physical_device_, *device_, vk_extent,
                                                 &raw_image, &raw_memory, &raw_view);
      if (result != vk::Result::eSuccess) { return std::unexpected(Error::platform_error); }
      target.shadow_depth_memory = vk::raii::DeviceMemory{device_, static_cast<VkDeviceMemory>(raw_memory)};
      target.shadow_depth_image = vk::raii::Image{device_, static_cast<VkImage>(raw_image)};
      target.shadow_depth_view = vk::raii::ImageView{device_, static_cast<VkImageView>(raw_view)};

      auto raw_sampler = vk::Sampler{};
      result = low::createShadowSampler(*device_, &raw_sampler);
      if (result != vk::Result::eSuccess) { return std::unexpected(Error::platform_error); }
      target.shadow_sampler = vk::raii::Sampler{device_, static_cast<VkSampler>(raw_sampler)};

      target.shadow_depth_layout = vk::ImageLayout::eUndefined;
      scene_shadow_depths_.assign(static_cast<std::size_t>(extent.width) * extent.height, 1.0F);
      const auto bytes = std::as_writable_bytes(std::span{scene_shadow_depths_});
      auto raw_buffer = vk::Buffer{};
      auto raw_buffer_memory = vk::DeviceMemory{};
      result = low::createHostBuffer(*physical_device_, *device_, bytes.size(),
                                     vk::BufferUsageFlagBits::eTransferDst, &raw_buffer, &raw_buffer_memory);
      if (result == vk::Result::eSuccess) {
         target.shadow_readback_memory =
            vk::raii::DeviceMemory{device_, static_cast<VkDeviceMemory>(raw_buffer_memory)};
         target.shadow_readback = vk::raii::Buffer{device_, static_cast<VkBuffer>(raw_buffer)};
      }
      return result == vk::Result::eSuccess ? std::expected<void, Error>{} : std::unexpected(Error::platform_error);
   }

   /// @brief Creates the storage buffer and descriptor used by shader debug samples.
   std::expected<void, Error> FrameHost::createSceneDebugTarget(FrameTarget &target) {
      if (detail::has(target.scene_debug_buffer)) { return {}; }
      const auto size = vk::DeviceSize{sizeof(SceneDebugSample) * scene_debug_samples_.size()};
      auto raw_buffer = vk::Buffer{};
      auto raw_memory = vk::DeviceMemory{};
      auto result = low::createHostBuffer(*physical_device_, *device_, size, vk::BufferUsageFlagBits::eStorageBuffer,
                                          &raw_buffer, &raw_memory);
      if (result != vk::Result::eSuccess) { return std::unexpected(Error::platform_error); }
      target.scene_debug_memory = vk::raii::DeviceMemory{device_, static_cast<VkDeviceMemory>(raw_memory)};
      target.scene_debug_buffer = vk::raii::Buffer{device_, static_cast<VkBuffer>(raw_buffer)};

      auto raw_layout = vk::DescriptorSetLayout{};
      auto raw_pool = vk::DescriptorPool{};
      result = low::createSceneDescriptor(*device_, *target.scene_debug_buffer, size, *target.shadow_depth_view,
                                          *target.shadow_sampler, &raw_layout, &raw_pool,
                                          &target.scene_debug_set);
      if (result == vk::Result::eSuccess) {
         target.scene_debug_layout =
            vk::raii::DescriptorSetLayout{device_, static_cast<VkDescriptorSetLayout>(raw_layout)};
         target.scene_debug_pool = vk::raii::DescriptorPool{device_, static_cast<VkDescriptorPool>(raw_pool)};
      }
      return result == vk::Result::eSuccess ? std::expected<void, Error>{} : std::unexpected(Error::platform_error);
   }

   /// @brief Resets shader debug samples before recording a scene draw.
   std::expected<void, Error> FrameHost::clearSceneDebugTarget(FrameTarget &target) {
      scene_debug_samples_.fill(SceneDebugSample{});
      const auto bytes = std::as_bytes(std::span{scene_debug_samples_});
      const auto result = low::writeBuffer(*device_, *target.scene_debug_memory, bytes);
      return result == vk::Result::eSuccess ? std::expected<void, Error>{} : std::unexpected(Error::platform_error);
   }

   /// @brief Reads shader debug samples after GPU execution has completed.
   std::expected<void, Error> FrameHost::readSceneDebugTarget(FrameTarget &target) {
      const auto bytes = std::as_writable_bytes(std::span{scene_debug_samples_});
      const auto result = low::readBuffer(*device_, *target.scene_debug_memory, bytes);
      return result == vk::Result::eSuccess ? std::expected<void, Error>{} : std::unexpected(Error::platform_error);
   }

   /// @brief Acquires one swapchain image, lets a caller record work, submits, waits, and presents.
   template <typename TRecord>
   std::expected<void, Error> FrameHost::presentFrame(FrameTarget &target, TRecord &&record) {
      const auto acquire_fence = *acquire_fence_;
      if ((*device_).resetFences(1, &acquire_fence) != vk::Result::eSuccess) {
         return std::unexpected(Error::platform_error);
      }

      std::uint32_t image_index{};
      auto result = (*device_).acquireNextImageKHR(*target.swapchain, std::numeric_limits<std::uint64_t>::max(),
                                                   nullptr, acquire_fence, &image_index);
      if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
         return std::unexpected(Error::platform_error);
      }
      result = (*device_).waitForFences(1, &acquire_fence, VK_TRUE, std::numeric_limits<std::uint64_t>::max());
      if (result != vk::Result::eSuccess || image_index >= target.images.size()) {
         return std::unexpected(Error::platform_error);
      }

      result = std::forward<TRecord>(record)(image_index);
      if (result != vk::Result::eSuccess) { return std::unexpected(Error::platform_error); }

      result = low::submitAndWait(*device_, *queue_, *command_buffer_, *frame_timeline_, ++timeline_value_);
      if (result != vk::Result::eSuccess) { return std::unexpected(Error::platform_error); }

      const auto swapchain = *target.swapchain;
      auto present = vk::PresentInfoKHR{};
      present.swapchainCount = 1;
      present.pSwapchains = &swapchain;
      present.pImageIndices = &image_index;
      result = (*queue_).presentKHR(&present);
      if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
         return std::unexpected(Error::platform_error);
      }
      target.layouts[image_index] = vk::ImageLayout::ePresentSrcKHR;
      return {};
   }

   /// @brief Acquires, clears, submits, waits, and presents one target image.
   std::expected<void, Error> FrameHost::clearTarget(FrameTarget &target, const vk::ClearColorValue &color) {
      return presentFrame(target, [&](std::uint32_t image_index) {
         return low::recordSwapchainClear(*device_, *command_pool_, *command_buffer_, target.images[image_index],
                                          *target.views[image_index],
                                          vk::Extent2D{target.extent.width, target.extent.height},
                                          target.layouts[image_index], color);
      });
   }

   /// @brief Acquires, clears, draws a triangle, submits, waits, and presents one target image.
   std::expected<void, Error> FrameHost::drawTriangleTarget(FrameTarget &target, const vk::ClearColorValue &color) {
      return presentFrame(target, [&](std::uint32_t image_index) {
         return low::recordSwapchainTriangle(*device_, *command_pool_, *command_buffer_, target.images[image_index],
                                             *target.views[image_index],
                                             vk::Extent2D{target.extent.width, target.extent.height},
                                             target.layouts[image_index], *target.triangle_pipeline, color);
      });
   }

   /// @brief Acquires, clears, draws uploaded scene geometry, submits, waits, and presents one target image.
   std::expected<void, Error>
   FrameHost::drawSceneTarget(FrameTarget &target, const vk::ClearColorValue &color,
                              std::span<const float> scene_constants) {
      const auto result = presentFrame(target, [&](std::uint32_t image_index) {
         return low::recordSwapchainScene(*device_, *command_pool_, *command_buffer_, target.images[image_index],
                                          *target.views[image_index],
                                          vk::Extent2D{target.extent.width, target.extent.height},
                                          target.layouts[image_index], *target.depth_image, *target.depth_view,
                                          target.depth_layout, *target.shadow_depth_image,
                                          target.shadow_depth_layout, *target.scene_layout,
                                          *target.scene_pipeline, *target.scene_vertices, *target.scene_indices,
                                          target.scene_debug_set, *target.scene_debug_buffer,
                                          sizeof(SceneDebugSample) * scene_debug_samples_.size(),
                                          target.scene_index_count, scene_constants, color);
      });
      if (!result) { return result; }
      target.depth_layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
      target.shadow_depth_layout = vk::ImageLayout::eShaderReadOnlyOptimal;
      return {};
   }

   /// @brief Draws uploaded scene geometry into the shadow depth image and reads it back.
   std::expected<void, Error>
   FrameHost::drawSceneShadowTarget(FrameTarget &target, std::span<const float> scene_constants) {
      const auto extent = sceneShadowExtent();
      auto result = low::recordSceneShadowDepth(*device_, *command_pool_, *command_buffer_,
                                                *target.shadow_depth_image, *target.shadow_depth_view,
                                                vk::Extent2D{extent.width, extent.height},
                                                target.shadow_depth_layout, *target.shadow_layout,
                                                *target.shadow_pipeline, *target.scene_vertices,
                                                *target.scene_indices, target.scene_index_count,
                                                scene_constants, *target.shadow_readback);
      if (result != vk::Result::eSuccess) { return std::unexpected(Error::platform_error); }

      result = low::submitAndWait(*device_, *queue_, *command_buffer_, *frame_timeline_, ++timeline_value_);
      if (result != vk::Result::eSuccess) { return std::unexpected(Error::platform_error); }

      target.shadow_depth_layout = vk::ImageLayout::eTransferSrcOptimal;
      const auto bytes = std::as_writable_bytes(std::span{scene_shadow_depths_});
      result = low::readBuffer(*device_, *target.shadow_readback_memory, bytes);
      return result == vk::Result::eSuccess ? std::expected<void, Error>{} : std::unexpected(Error::platform_error);
   }

   /// @brief Checks whether prepared targets still match current native windows.
   bool FrameHost::matches(std::span<const std::reference_wrapper<Window>> windows) const {
      if (targets_.size() != windows.size()) { return false; }
      return std::ranges::equal(targets_, windows, {}, &FrameTarget::window,
                                [](const std::reference_wrapper<Window> &window) {
                                   return window.get().handle();
                                }) &&
             std::ranges::equal(targets_, windows, [](PixelExtent a, const std::reference_wrapper<Window> &window) {
                const auto b = window.get().extent();
                return a.width == b.width && a.height == b.height;
             }, &FrameTarget::extent);
   }

   /// @brief Destroys reusable frame command and timeline synchronization objects.
   void FrameHost::clearFrameExecutor() {
      if (detail::has(device_)) { static_cast<void>((*device_).waitIdle()); }
      acquire_fence_.clear();
      frame_timeline_.clear();
      command_buffer_.clear();
      command_pool_.clear();
      timeline_value_ = 0;
   }

   /// @brief Destroys swapchains, depth images, image views, and presentation surfaces.
   void FrameHost::clearTargets() {
      if (detail::has(device_)) { static_cast<void>((*device_).waitIdle()); }
      targets_.clear();
   }

   /// @brief Destroys every Vulkan object owned by this frame host.
   void FrameHost::reset() {
      clearTargets();
      clearFrameExecutor();
      queue_.clear();
      device_.clear();
      physical_device_.clear();
      queue_family_ = 0;
      scene_shadow_depths_.clear();
      instance_.clear();
   }

} // namespace vve::v4::vh
