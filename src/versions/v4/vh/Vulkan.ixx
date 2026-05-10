module;
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.hpp>

export module VEEngine.V4:Vulkan;
import std;
import :VulkanLow;
import :Window;

/// @file
/// @brief Stateful Vulkan-Hpp helper objects built on top of the stateless vh layer.

export namespace vve::v4::vh {

   /// @brief Vulkan objects associated with one drawable window.
   struct FrameTarget {
      WindowHandle window{};                  ///< Engine window owning this target.
      PixelExtent extent{};                   ///< Current drawable extent.
      vk::SurfaceKHR surface{};               ///< Platform presentation surface.
      vk::SwapchainKHR swapchain{};           ///< Swapchain used by the renderer.
      vk::Format color_format{};              ///< Swapchain image format.
      vk::Format depth_format{};              ///< Depth image format.
      std::vector<vk::Image> images{};        ///< Swapchain images.
      std::vector<vk::ImageView> views{};     ///< Swapchain color views.
      vk::Image depth_image{};                ///< Depth attachment image.
      vk::DeviceMemory depth_memory{};        ///< Device-local depth memory.
      vk::ImageView depth_view{};             ///< Depth attachment view.
      vk::ImageLayout depth_layout{};         ///< Current depth image layout.
      std::vector<vk::ImageLayout> layouts{}; ///< Current swapchain image layouts.
      vk::PipelineLayout triangle_layout{};   ///< Pipeline layout for the smoke-test triangle.
      vk::Pipeline triangle_pipeline{};        ///< Pipeline used to draw the smoke-test triangle.
      vk::PipelineLayout scene_layout{};      ///< Pipeline layout for uploaded unlit scene geometry.
      vk::Pipeline scene_pipeline{};          ///< Pipeline used to draw uploaded unlit scene geometry.
      vk::Buffer scene_vertices{};            ///< Host-visible position/color vertex buffer.
      vk::DeviceMemory scene_vertex_memory{}; ///< Memory backing the scene vertex buffer.
      vk::Buffer scene_indices{};             ///< Host-visible index buffer.
      vk::DeviceMemory scene_index_memory{};  ///< Memory backing the scene index buffer.
      vk::DescriptorSetLayout scene_debug_layout{}; ///< Layout for shader debug readback.
      vk::DescriptorPool scene_debug_pool{};         ///< Pool owning the debug descriptor set.
      vk::DescriptorSet scene_debug_set{};           ///< Storage-buffer descriptor for debug readback.
      vk::Buffer scene_debug_buffer{};               ///< Host-visible shader debug buffer.
      vk::DeviceMemory scene_debug_memory{};         ///< Memory backing the shader debug buffer.
      std::uint32_t scene_vertex_count{};      ///< Uploaded vertex count.
      std::uint32_t scene_index_count{};       ///< Uploaded index count.
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
      std::array<float, 3> normal{};                       ///< GPU-normalized surface normal.
      float n_dot_l{};                                     ///< GPU-computed Lambert term.
      std::array<float, 3> direction_to_light{};           ///< GPU-normalized direction to light.
      float intensity{};                                   ///< Direct-light intensity.
      std::array<float, 3> ambient_lighting{};             ///< Ambient light contribution.
      float direct_factor{};                               ///< Direct-light scalar factor.
      std::array<float, 3> direct_lighting{};              ///< Direct light contribution.
      float unused0{};                                     ///< Layout padding visible to the host.
      std::array<float, 3> final_lighting{};               ///< Ambient plus direct lighting.
      float unused1{};                                     ///< Layout padding visible to the host.
   };
   static_assert(sizeof(SceneDebugSample) == 176);

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
                  std::string_view fragment_entry, std::span<const SceneVertex> vertices,
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
      uploadScene(FrameTarget &target, std::span<const SceneVertex> vertices,
                  std::span<const std::uint32_t> indices);
      [[nodiscard]] std::expected<void, Error> createSceneDebugTarget(FrameTarget &target);
      [[nodiscard]] std::expected<void, Error> clearSceneDebugTarget(FrameTarget &target);
      [[nodiscard]] std::expected<void, Error> readSceneDebugTarget(FrameTarget &target);
      [[nodiscard]] std::expected<void, Error> drawTriangleTarget(FrameTarget &target,
                                                                  const vk::ClearColorValue &color);
      [[nodiscard]] std::expected<void, Error> drawSceneTarget(FrameTarget &target,
                                                               const vk::ClearColorValue &color,
                                                               std::span<const float> scene_constants);
      [[nodiscard]] bool matches(std::span<const std::reference_wrapper<Window>> windows) const;
      void destroyFrameExecutor();
      void destroyTargets();
      void reset();

      vk::Instance instance_{};
      vk::PhysicalDevice physical_device_{};
      vk::Device device_{};
      vk::Queue queue_{};
      std::uint32_t queue_family_{};
      vk::CommandPool command_pool_{};
      vk::CommandBuffer command_buffer_{};
      vk::Semaphore frame_timeline_{};
      vk::Fence acquire_fence_{};
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
      std::array<float, 4> last_clear_color_{};
   };

} // namespace vve::v4::vh

namespace vve::v4::vh {

   /// @brief Releases all owned Vulkan objects.
   FrameHost::~FrameHost() { reset(); }

   /// @brief Moves ownership of all Vulkan objects.
   FrameHost::FrameHost(FrameHost &&other) noexcept
       : instance_{std::exchange(other.instance_, nullptr)},
         physical_device_{std::exchange(other.physical_device_, nullptr)},
         device_{std::exchange(other.device_, nullptr)},
         queue_{std::exchange(other.queue_, nullptr)},
         queue_family_{std::exchange(other.queue_family_, 0)},
         command_pool_{std::exchange(other.command_pool_, nullptr)},
         command_buffer_{std::exchange(other.command_buffer_, nullptr)},
         frame_timeline_{std::exchange(other.frame_timeline_, nullptr)},
         acquire_fence_{std::exchange(other.acquire_fence_, nullptr)},
         targets_{std::move(other.targets_)},
         timeline_value_{std::exchange(other.timeline_value_, 0)},
         presented_frames_{std::exchange(other.presented_frames_, 0)},
         triangle_draws_{std::exchange(other.triangle_draws_, 0)},
         scene_uploads_{std::exchange(other.scene_uploads_, 0)},
         scene_mesh_draws_{std::exchange(other.scene_mesh_draws_, 0)},
         scene_instance_draws_{std::exchange(other.scene_instance_draws_, 0)},
         scene_vertex_count_{std::exchange(other.scene_vertex_count_, 0)},
         scene_index_count_{std::exchange(other.scene_index_count_, 0)},
         scene_debug_samples_{std::exchange(other.scene_debug_samples_, {})},
         last_clear_color_{std::exchange(other.last_clear_color_, {})} {}

   /// @brief Moves ownership of all Vulkan objects.
   FrameHost &FrameHost::operator=(FrameHost &&other) noexcept {
      if (this != std::addressof(other)) {
         reset();
         instance_ = std::exchange(other.instance_, nullptr);
         physical_device_ = std::exchange(other.physical_device_, nullptr);
         device_ = std::exchange(other.device_, nullptr);
         queue_ = std::exchange(other.queue_, nullptr);
         queue_family_ = std::exchange(other.queue_family_, 0);
         command_pool_ = std::exchange(other.command_pool_, nullptr);
         command_buffer_ = std::exchange(other.command_buffer_, nullptr);
         frame_timeline_ = std::exchange(other.frame_timeline_, nullptr);
         acquire_fence_ = std::exchange(other.acquire_fence_, nullptr);
         targets_ = std::move(other.targets_);
         timeline_value_ = std::exchange(other.timeline_value_, 0);
         presented_frames_ = std::exchange(other.presented_frames_, 0);
         triangle_draws_ = std::exchange(other.triangle_draws_, 0);
         scene_uploads_ = std::exchange(other.scene_uploads_, 0);
         scene_mesh_draws_ = std::exchange(other.scene_mesh_draws_, 0);
         scene_instance_draws_ = std::exchange(other.scene_instance_draws_, 0);
         scene_vertex_count_ = std::exchange(other.scene_vertex_count_, 0);
         scene_index_count_ = std::exchange(other.scene_index_count_, 0);
         scene_debug_samples_ = std::exchange(other.scene_debug_samples_, {});
         last_clear_color_ = std::exchange(other.last_clear_color_, {});
      }
      return *this;
   }

   /// @brief Ensures every visible native SDL window has a swapchain and depth image.
   std::expected<void, Error> FrameHost::prepare(WindowSystem &windows) {
      auto refs = windows.windows();
      auto native = std::vector<std::reference_wrapper<Window>>{};
      for (Window &window : refs | std::views::transform([](auto ref) -> Window & { return ref.get(); })) {
         if (window.native() != nullptr && !window.shouldClose() && !window.minimized()) { native.push_back(window); }
      }
      if (native.empty()) { destroyTargets(); return {}; }
      if (device_ && matches(native)) { return {}; }
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
                          std::string_view fragment_entry, std::span<const SceneVertex> vertices,
                          std::span<const std::uint32_t> indices, std::uint32_t mesh_count,
                          std::uint32_t instance_count, std::span<const float> scene_constants) {
      if (!ready()) { return {}; }
      if (vertices.empty() || indices.empty() || scene_constants.size() < 32) {
         return std::unexpected(Error::invalid_argument);
      }

      const auto clear = vk::ClearColorValue{color};
      for (auto &target : targets_) {
         if (const auto result = createSceneDebugTarget(target); !result) { return result; }
         if (const auto result = createScenePipeline(target, vertex_spirv, vertex_entry,
                                                     fragment_spirv, fragment_entry); !result) {
            return result;
         }
         if (const auto result = uploadScene(target, vertices, indices); !result) { return result; }
         if (const auto result = clearSceneDebugTarget(target); !result) { return result; }
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
   bool FrameHost::ready() const { return static_cast<bool>(instance_ && device_ && !targets_.empty()); }

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

   /// @brief Returns the fixed clear color used by the most recent clear frame.
   std::array<float, 4> FrameHost::lastClearColor() const { return last_clear_color_; }

   /// @brief Creates the Vulkan instance using SDL's required platform extensions.
   std::expected<void, Error> FrameHost::createInstance() {
      std::uint32_t sdl_count{};
      const char *const *sdl_extensions = SDL_Vulkan_GetInstanceExtensions(&sdl_count);
      if (sdl_extensions == nullptr) { return std::unexpected(Error::platform_error); }
      const auto extensions = std::span<const char *const>{sdl_extensions, sdl_count};
      const auto result = low::createInstance("ViennaVulkanEngine v4", extensions, &instance_);
      return result == vk::Result::eSuccess ? std::expected<void, Error>{} : std::unexpected(Error::platform_error);
   }

   /// @brief Rebuilds surfaces, swapchains, and depth targets for the current native windows.
   std::expected<void, Error> FrameHost::rebuildTargets(std::span<std::reference_wrapper<Window>> windows) {
      if (!instance_) {
         if (const auto result = createInstance(); !result) { return result; }
      }
      destroyTargets();

      auto surfaces = std::vector<vk::SurfaceKHR>{};
      for (Window &window : windows | std::views::transform([](auto ref) -> Window & { return ref.get(); })) {
         auto &target = targets_.emplace_back(FrameTarget{.window = window.handle(), .extent = window.extent()});
         if (const auto result = createSurface(target, window); !result) { reset(); return result; }
         surfaces.push_back(target.surface);
      }

      if (!device_) {
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
      const auto raw_instance = static_cast<VkInstance>(instance_);
      if (!SDL_Vulkan_CreateSurface(window.native(), raw_instance, nullptr, &raw_surface)) {
         return std::unexpected(Error::platform_error);
      }
      target.surface = raw_surface;
      if (!device_) { return {}; }

      vk::Bool32 supported{};
      const auto result = physical_device_.getSurfaceSupportKHR(queue_family_, target.surface, &supported);
      if (result != vk::Result::eSuccess || supported != VK_TRUE) { return std::unexpected(Error::platform_error); }
      return {};
   }

   /// @brief Creates the logical Vulkan device selected by the stateless helper layer.
   std::expected<void, Error> FrameHost::createDevice(std::span<const vk::SurfaceKHR> surfaces) {
      auto extensions = std::vector<std::string>{};
      if (!low::chooseDevice(instance_, surfaces, &physical_device_, &queue_family_, &extensions)) {
         return std::unexpected(Error::platform_error);
      }
      const auto result = low::createDevice(physical_device_, queue_family_, extensions, &device_, &queue_);
      if (result != vk::Result::eSuccess) { return std::unexpected(Error::platform_error); }
      return createFrameExecutor();
   }

   /// @brief Creates swapchain images and color views for one frame target.
   std::expected<void, Error> FrameHost::createSwapchain(FrameTarget &target, Window &window) {
      const auto fallback = vk::Extent2D{window.extent().width, window.extent().height};
      auto extent = vk::Extent2D{};
      const auto result = low::createSwapchainAndViews(physical_device_, device_, target.surface, fallback,
                                                       &target.swapchain, &target.color_format, &extent,
                                                       &target.images, &target.views);
      target.extent = PixelExtent{.width = extent.width, .height = extent.height};
      target.layouts.assign(target.images.size(), vk::ImageLayout::eUndefined);
      return result == vk::Result::eSuccess ? std::expected<void, Error>{} : std::unexpected(Error::platform_error);
   }

   /// @brief Creates a depth attachment for one frame target.
   std::expected<void, Error> FrameHost::createDepth(FrameTarget &target) {
      const auto extent = vk::Extent2D{target.extent.width, target.extent.height};
      const auto result = low::createDepthTarget(physical_device_, device_, extent, &target.depth_format,
                                                 &target.depth_image, &target.depth_memory, &target.depth_view);
      target.depth_layout = vk::ImageLayout::eUndefined;
      return result == vk::Result::eSuccess ? std::expected<void, Error>{} : std::unexpected(Error::platform_error);
   }

   /// @brief Creates reusable frame command and timeline synchronization objects.
   std::expected<void, Error> FrameHost::createFrameExecutor() {
      const auto result = low::createFrameExecutor(device_, queue_family_, &command_pool_, &command_buffer_,
                                                   &frame_timeline_, &acquire_fence_);
      return result == vk::Result::eSuccess ? std::expected<void, Error>{} : std::unexpected(Error::platform_error);
   }

   /// @brief Creates the smoke-test triangle pipeline for one target if needed.
   std::expected<void, Error>
   FrameHost::createTrianglePipeline(FrameTarget &target, std::span<const std::uint32_t> vertex_spirv,
                                     std::string_view vertex_entry,
                                     std::span<const std::uint32_t> fragment_spirv,
                                     std::string_view fragment_entry) {
      if (target.triangle_pipeline) { return {}; }
      const auto result = low::createTrianglePipeline(device_, target.color_format, vertex_spirv, vertex_entry,
                                                      fragment_spirv, fragment_entry,
                                                      &target.triangle_layout, &target.triangle_pipeline);
      return result == vk::Result::eSuccess ? std::expected<void, Error>{} : std::unexpected(Error::platform_error);
   }

   /// @brief Creates the uploaded-scene pipeline for one target if needed.
   std::expected<void, Error>
   FrameHost::createScenePipeline(FrameTarget &target, std::span<const std::uint32_t> vertex_spirv,
                                  std::string_view vertex_entry,
                                  std::span<const std::uint32_t> fragment_spirv,
                                  std::string_view fragment_entry) {
      if (target.scene_pipeline) { return {}; }
      const auto result = low::createScenePipeline(device_, target.color_format, target.depth_format,
                                                   target.scene_debug_layout, vertex_spirv, vertex_entry,
                                                   fragment_spirv, fragment_entry,
                                                   &target.scene_layout, &target.scene_pipeline);
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
      if (!target.scene_vertices || !target.scene_indices || !same_size) {
         if (target.scene_vertices) { device_.destroyBuffer(target.scene_vertices); }
         if (target.scene_vertex_memory) { device_.freeMemory(target.scene_vertex_memory); }
         if (target.scene_indices) { device_.destroyBuffer(target.scene_indices); }
         if (target.scene_index_memory) { device_.freeMemory(target.scene_index_memory); }
         target.scene_vertices = nullptr;
         target.scene_vertex_memory = nullptr;
         target.scene_indices = nullptr;
         target.scene_index_memory = nullptr;

         auto created = low::createHostBuffer(physical_device_, device_, vertex_bytes.size(),
                                              vk::BufferUsageFlagBits::eVertexBuffer,
                                              &target.scene_vertices, &target.scene_vertex_memory);
         if (created != vk::Result::eSuccess) { return std::unexpected(Error::platform_error); }
         created = low::createHostBuffer(physical_device_, device_, index_bytes.size(),
                                         vk::BufferUsageFlagBits::eIndexBuffer,
                                         &target.scene_indices, &target.scene_index_memory);
         if (created != vk::Result::eSuccess) { return std::unexpected(Error::platform_error); }
      }

      auto result = low::writeBuffer(device_, target.scene_vertex_memory, vertex_bytes);
      if (result != vk::Result::eSuccess) { return std::unexpected(Error::platform_error); }
      result = low::writeBuffer(device_, target.scene_index_memory, index_bytes);
      if (result != vk::Result::eSuccess) { return std::unexpected(Error::platform_error); }
      target.scene_vertex_count = static_cast<std::uint32_t>(vertices.size());
      target.scene_index_count = static_cast<std::uint32_t>(indices.size());
      return {};
   }

   /// @brief Creates the storage buffer and descriptor used by shader debug samples.
   std::expected<void, Error> FrameHost::createSceneDebugTarget(FrameTarget &target) {
      if (target.scene_debug_buffer) { return {}; }
      const auto size = vk::DeviceSize{sizeof(SceneDebugSample) * scene_debug_samples_.size()};
      auto result = low::createHostBuffer(physical_device_, device_, size, vk::BufferUsageFlagBits::eStorageBuffer,
                                          &target.scene_debug_buffer, &target.scene_debug_memory);
      if (result != vk::Result::eSuccess) { return std::unexpected(Error::platform_error); }
      result = low::createStorageDescriptor(device_, target.scene_debug_buffer, size, &target.scene_debug_layout,
                                            &target.scene_debug_pool, &target.scene_debug_set);
      return result == vk::Result::eSuccess ? std::expected<void, Error>{} : std::unexpected(Error::platform_error);
   }

   /// @brief Resets shader debug samples before recording a scene draw.
   std::expected<void, Error> FrameHost::clearSceneDebugTarget(FrameTarget &target) {
      scene_debug_samples_.fill(SceneDebugSample{});
      const auto bytes = std::as_bytes(std::span{scene_debug_samples_});
      const auto result = low::writeBuffer(device_, target.scene_debug_memory, bytes);
      return result == vk::Result::eSuccess ? std::expected<void, Error>{} : std::unexpected(Error::platform_error);
   }

   /// @brief Reads shader debug samples after GPU execution has completed.
   std::expected<void, Error> FrameHost::readSceneDebugTarget(FrameTarget &target) {
      const auto bytes = std::as_writable_bytes(std::span{scene_debug_samples_});
      const auto result = low::readBuffer(device_, target.scene_debug_memory, bytes);
      return result == vk::Result::eSuccess ? std::expected<void, Error>{} : std::unexpected(Error::platform_error);
   }

   /// @brief Acquires, clears, submits, waits, and presents one target image.
   std::expected<void, Error> FrameHost::clearTarget(FrameTarget &target, const vk::ClearColorValue &color) {
      if (device_.resetFences(1, &acquire_fence_) != vk::Result::eSuccess) {
         return std::unexpected(Error::platform_error);
      }

      std::uint32_t image_index{};
      auto result = device_.acquireNextImageKHR(target.swapchain, std::numeric_limits<std::uint64_t>::max(),
                                                nullptr, acquire_fence_, &image_index);
      if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
         return std::unexpected(Error::platform_error);
      }
      result = device_.waitForFences(1, &acquire_fence_, VK_TRUE, std::numeric_limits<std::uint64_t>::max());
      if (result != vk::Result::eSuccess || image_index >= target.images.size()) {
         return std::unexpected(Error::platform_error);
      }

      const auto old_layout = target.layouts[image_index];
      result = low::recordSwapchainClear(device_, command_pool_, command_buffer_, target.images[image_index],
                                         target.views[image_index],
                                         vk::Extent2D{target.extent.width, target.extent.height},
                                         old_layout, color);
      if (result != vk::Result::eSuccess) { return std::unexpected(Error::platform_error); }

      result = low::submitAndWait(device_, queue_, command_buffer_, frame_timeline_, ++timeline_value_);
      if (result != vk::Result::eSuccess) { return std::unexpected(Error::platform_error); }

      auto present = vk::PresentInfoKHR{};
      present.swapchainCount = 1;
      present.pSwapchains = &target.swapchain;
      present.pImageIndices = &image_index;
      result = queue_.presentKHR(&present);
      if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
         return std::unexpected(Error::platform_error);
      }
      target.layouts[image_index] = vk::ImageLayout::ePresentSrcKHR;
      return {};
   }

   /// @brief Acquires, clears, draws a triangle, submits, waits, and presents one target image.
   std::expected<void, Error> FrameHost::drawTriangleTarget(FrameTarget &target, const vk::ClearColorValue &color) {
      if (device_.resetFences(1, &acquire_fence_) != vk::Result::eSuccess) {
         return std::unexpected(Error::platform_error);
      }

      std::uint32_t image_index{};
      auto result = device_.acquireNextImageKHR(target.swapchain, std::numeric_limits<std::uint64_t>::max(),
                                                nullptr, acquire_fence_, &image_index);
      if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
         return std::unexpected(Error::platform_error);
      }
      result = device_.waitForFences(1, &acquire_fence_, VK_TRUE, std::numeric_limits<std::uint64_t>::max());
      if (result != vk::Result::eSuccess || image_index >= target.images.size()) {
         return std::unexpected(Error::platform_error);
      }

      result = low::recordSwapchainTriangle(device_, command_pool_, command_buffer_, target.images[image_index],
                                            target.views[image_index],
                                            vk::Extent2D{target.extent.width, target.extent.height},
                                            target.layouts[image_index], target.triangle_pipeline, color);
      if (result != vk::Result::eSuccess) { return std::unexpected(Error::platform_error); }

      result = low::submitAndWait(device_, queue_, command_buffer_, frame_timeline_, ++timeline_value_);
      if (result != vk::Result::eSuccess) { return std::unexpected(Error::platform_error); }

      auto present = vk::PresentInfoKHR{};
      present.swapchainCount = 1;
      present.pSwapchains = &target.swapchain;
      present.pImageIndices = &image_index;
      result = queue_.presentKHR(&present);
      if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
         return std::unexpected(Error::platform_error);
      }
      target.layouts[image_index] = vk::ImageLayout::ePresentSrcKHR;
      return {};
   }

   /// @brief Acquires, clears, draws uploaded scene geometry, submits, waits, and presents one target image.
   std::expected<void, Error>
   FrameHost::drawSceneTarget(FrameTarget &target, const vk::ClearColorValue &color,
                              std::span<const float> scene_constants) {
      if (device_.resetFences(1, &acquire_fence_) != vk::Result::eSuccess) {
         return std::unexpected(Error::platform_error);
      }

      std::uint32_t image_index{};
      auto result = device_.acquireNextImageKHR(target.swapchain, std::numeric_limits<std::uint64_t>::max(),
                                                nullptr, acquire_fence_, &image_index);
      if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
         return std::unexpected(Error::platform_error);
      }
      result = device_.waitForFences(1, &acquire_fence_, VK_TRUE, std::numeric_limits<std::uint64_t>::max());
      if (result != vk::Result::eSuccess || image_index >= target.images.size()) {
         return std::unexpected(Error::platform_error);
      }

      result = low::recordSwapchainScene(device_, command_pool_, command_buffer_, target.images[image_index],
                                         target.views[image_index],
                                         vk::Extent2D{target.extent.width, target.extent.height},
                                         target.layouts[image_index], target.depth_image, target.depth_view,
                                         target.depth_layout, target.scene_layout, target.scene_pipeline,
                                         target.scene_vertices, target.scene_indices, target.scene_debug_set,
                                         target.scene_debug_buffer,
                                         sizeof(SceneDebugSample) * scene_debug_samples_.size(),
                                         target.scene_index_count, scene_constants, color);
      if (result != vk::Result::eSuccess) { return std::unexpected(Error::platform_error); }

      result = low::submitAndWait(device_, queue_, command_buffer_, frame_timeline_, ++timeline_value_);
      if (result != vk::Result::eSuccess) { return std::unexpected(Error::platform_error); }

      auto present = vk::PresentInfoKHR{};
      present.swapchainCount = 1;
      present.pSwapchains = &target.swapchain;
      present.pImageIndices = &image_index;
      result = queue_.presentKHR(&present);
      if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
         return std::unexpected(Error::platform_error);
      }
      target.layouts[image_index] = vk::ImageLayout::ePresentSrcKHR;
      target.depth_layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
      return {};
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
   void FrameHost::destroyFrameExecutor() {
      if (device_) { static_cast<void>(device_.waitIdle()); }
      if (acquire_fence_) { device_.destroyFence(acquire_fence_); }
      if (frame_timeline_) { device_.destroySemaphore(frame_timeline_); }
      if (command_pool_) { device_.destroyCommandPool(command_pool_); }
      command_buffer_ = nullptr;
      command_pool_ = nullptr;
      frame_timeline_ = nullptr;
      acquire_fence_ = nullptr;
      timeline_value_ = 0;
   }

   /// @brief Destroys swapchains, depth images, image views, and presentation surfaces.
   void FrameHost::destroyTargets() {
      if (device_) { static_cast<void>(device_.waitIdle()); }
      for (auto &target : std::views::reverse(targets_)) {
         if (target.triangle_pipeline) { device_.destroyPipeline(target.triangle_pipeline); }
         if (target.triangle_layout) { device_.destroyPipelineLayout(target.triangle_layout); }
         if (target.scene_pipeline) { device_.destroyPipeline(target.scene_pipeline); }
         if (target.scene_layout) { device_.destroyPipelineLayout(target.scene_layout); }
         if (target.scene_vertices) { device_.destroyBuffer(target.scene_vertices); }
         if (target.scene_vertex_memory) { device_.freeMemory(target.scene_vertex_memory); }
         if (target.scene_indices) { device_.destroyBuffer(target.scene_indices); }
         if (target.scene_index_memory) { device_.freeMemory(target.scene_index_memory); }
         if (target.scene_debug_pool) { device_.destroyDescriptorPool(target.scene_debug_pool); }
         if (target.scene_debug_layout) { device_.destroyDescriptorSetLayout(target.scene_debug_layout); }
         if (target.scene_debug_buffer) { device_.destroyBuffer(target.scene_debug_buffer); }
         if (target.scene_debug_memory) { device_.freeMemory(target.scene_debug_memory); }
         if (target.depth_view) { device_.destroyImageView(target.depth_view); }
         if (target.depth_image) { device_.destroyImage(target.depth_image); }
         if (target.depth_memory) { device_.freeMemory(target.depth_memory); }
         for (const auto view : target.views) { device_.destroyImageView(view); }
         if (target.swapchain) { device_.destroySwapchainKHR(target.swapchain); }
         if (target.surface) {
            SDL_Vulkan_DestroySurface(static_cast<VkInstance>(instance_),
                                      static_cast<VkSurfaceKHR>(target.surface), nullptr);
         }
      }
      targets_.clear();
   }

   /// @brief Destroys every Vulkan object owned by this frame host.
   void FrameHost::reset() {
      destroyTargets();
      destroyFrameExecutor();
      if (device_) {
         device_.destroy();
         device_ = nullptr;
      }
      physical_device_ = nullptr;
      queue_ = nullptr;
      queue_family_ = 0;
      if (instance_) {
         instance_.destroy();
         instance_ = nullptr;
      }
   }

} // namespace vve::v4::vh
