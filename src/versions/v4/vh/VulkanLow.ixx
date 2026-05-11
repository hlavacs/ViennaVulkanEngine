module;
#include <cstdio>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_profiles.hpp>

export module VEEngine.V4:VulkanLow;
import std;

/// @file
/// @brief Stateless Vulkan-Hpp helper functions for v4 and other small projects.

export namespace vve::v4::vh::low {

   [[nodiscard]] bool hasExtension(std::span<const vk::ExtensionProperties> properties, std::string_view name);
   [[nodiscard]] std::vector<vk::ExtensionProperties> instanceExtensions();
   [[nodiscard]] std::vector<vk::ExtensionProperties> deviceExtensions(vk::PhysicalDevice device);
   [[nodiscard]] vk::SurfaceFormatKHR chooseSurfaceFormat(std::span<const vk::SurfaceFormatKHR> formats);
   [[nodiscard]] vk::Extent2D chooseExtent(const vk::SurfaceCapabilitiesKHR &caps, vk::Extent2D fallback);
   [[nodiscard]] vk::CompositeAlphaFlagBitsKHR chooseCompositeAlpha(vk::CompositeAlphaFlagsKHR supported);
   [[nodiscard]] std::optional<std::uint32_t>
   findMemoryType(vk::PhysicalDevice device, std::uint32_t bits, vk::MemoryPropertyFlags flags);
   [[nodiscard]] vk::Result
   createInstance(std::string_view app_name, std::span<const char *const> required_extensions,
                  vk::Instance *instance);
   [[nodiscard]] bool
   chooseDevice(vk::Instance instance, std::span<const vk::SurfaceKHR> surfaces, vk::PhysicalDevice *device,
                std::uint32_t *queue_family, std::vector<std::string> *extensions);
   [[nodiscard]] vk::Result
   createDevice(vk::PhysicalDevice physical_device, std::uint32_t queue_family,
                std::span<const std::string> extensions, vk::Device *device, vk::Queue *queue);
   [[nodiscard]] vk::Result
   createSwapchainAndViews(vk::PhysicalDevice physical_device, vk::Device device, vk::SurfaceKHR surface,
                           vk::Extent2D fallback_extent, vk::SwapchainKHR *swapchain, vk::Format *color_format,
                           vk::Extent2D *extent, std::vector<vk::Image> *images,
                           std::vector<vk::ImageView> *image_views);
   [[nodiscard]] vk::Result
   createDepthTarget(vk::PhysicalDevice physical_device, vk::Device device, vk::Extent2D extent, vk::Format *format,
                     vk::Image *image, vk::DeviceMemory *memory, vk::ImageView *view);
   [[nodiscard]] vk::Result
   createShadowDepthTarget(vk::PhysicalDevice physical_device, vk::Device device, vk::Extent2D extent,
                           vk::Image *image, vk::DeviceMemory *memory, vk::ImageView *view);
   [[nodiscard]] vk::Result createShadowSampler(vk::Device device, vk::Sampler *sampler);
   [[nodiscard]] vk::Result
   createImageTarget(vk::PhysicalDevice physical_device, vk::Device device, vk::Extent2D extent,
                     vk::Format format, vk::ImageUsageFlags usage, vk::FormatFeatureFlags features,
                     vk::ImageAspectFlags aspect, vk::Image *image, vk::DeviceMemory *memory,
                     vk::ImageView *view);
   [[nodiscard]] vk::Result
   createFrameExecutor(vk::Device device, std::uint32_t queue_family, vk::CommandPool *pool,
                       vk::CommandBuffer *command_buffer, vk::Semaphore *timeline, vk::Fence *acquire_fence);
   [[nodiscard]] vk::Result
   createHostBuffer(vk::PhysicalDevice physical_device, vk::Device device, vk::DeviceSize size,
                    vk::BufferUsageFlags usage, vk::Buffer *buffer, vk::DeviceMemory *memory);
   [[nodiscard]] vk::Result writeBuffer(vk::Device device, vk::DeviceMemory memory,
                                         std::span<const std::byte> bytes);
   [[nodiscard]] vk::Result readBuffer(vk::Device device, vk::DeviceMemory memory, std::span<std::byte> bytes);
   [[nodiscard]] vk::Result
   createSceneDescriptor(vk::Device device, vk::Buffer debug_buffer, vk::DeviceSize debug_size,
                         vk::ImageView shadow_view, vk::Sampler shadow_sampler,
                         vk::DescriptorSetLayout *layout, vk::DescriptorPool *pool, vk::DescriptorSet *set);
   [[nodiscard]] vk::Result
   recordSwapchainClear(vk::Device device, vk::CommandPool pool, vk::CommandBuffer command_buffer, vk::Image image,
                        vk::ImageView view, vk::Extent2D extent, vk::ImageLayout old_layout,
                        const vk::ClearColorValue &clear_color);
   [[nodiscard]] vk::Result
   createGraphicsPipeline(vk::Device device, std::span<const std::uint32_t> vertex_spirv,
                          std::string_view vertex_entry, std::span<const std::uint32_t> fragment_spirv,
                          std::string_view fragment_entry,
                          std::span<const vk::DescriptorSetLayout> set_layouts,
                          std::span<const vk::PushConstantRange> push_ranges,
                          std::span<const vk::VertexInputBindingDescription> vertex_bindings,
                          std::span<const vk::VertexInputAttributeDescription> vertex_attributes,
                          std::span<const vk::Format> color_formats, vk::Format depth_format,
                          bool depth_enabled, vk::PipelineLayout *layout, vk::Pipeline *pipeline);
   [[nodiscard]] vk::Result
   recordSwapchainTriangle(vk::Device device, vk::CommandPool pool, vk::CommandBuffer command_buffer,
                           vk::Image image, vk::ImageView view, vk::Extent2D extent, vk::ImageLayout old_layout,
                           vk::Pipeline pipeline, const vk::ClearColorValue &clear_color);
   [[nodiscard]] vk::Result
   recordSwapchainScene(vk::Device device, vk::CommandPool pool, vk::CommandBuffer command_buffer,
                        vk::Image image, vk::ImageView view, vk::Extent2D extent, vk::ImageLayout old_layout,
                        vk::Image depth_image, vk::ImageView depth_view, vk::ImageLayout depth_old_layout,
                        vk::Image shadow_image, vk::ImageLayout shadow_old_layout,
                        vk::PipelineLayout layout, vk::Pipeline pipeline, vk::Buffer vertex_buffer,
                        vk::Buffer index_buffer, vk::DescriptorSet debug_set, vk::Buffer debug_buffer,
                        vk::DeviceSize debug_buffer_size, std::uint32_t index_count,
                        std::span<const float> scene_constants, const vk::ClearColorValue &clear_color);
   [[nodiscard]] vk::Result
   recordSceneShadowDepth(vk::Device device, vk::CommandPool pool, vk::CommandBuffer command_buffer,
                          vk::Image depth_image, vk::ImageView depth_view, vk::Extent2D extent,
                          vk::ImageLayout old_layout, vk::PipelineLayout layout, vk::Pipeline pipeline,
                          vk::Buffer vertex_buffer, vk::Buffer index_buffer, std::uint32_t index_count,
                          std::span<const float> scene_constants, vk::Buffer readback_buffer);
   [[nodiscard]] vk::Result
   submitAndWait(vk::Device device, vk::Queue queue, vk::CommandBuffer command_buffer, vk::Semaphore timeline,
                 std::uint64_t value);

} // namespace vve::v4::vh::low

namespace vve::v4::vh::low::detail {

   struct SupportedProfile {
      VpProfileProperties profile{};
      std::uint32_t rank{};
   };

   struct ModernFeatures {
      bool timeline_semaphore{};
      bool synchronization2{};
      bool dynamic_rendering{};
      bool descriptor_indexing{};
      bool sampled_image_update_after_bind{};
      bool storage_buffer_update_after_bind{};
      bool partially_bound{};
      bool variable_descriptor_count{};
      bool runtime_descriptor_array{};
      bool descriptor_buffer{};
      bool descriptor_buffer_image_layout_ignored{};
      bool descriptor_buffer_push_descriptors{};
      bool maintenance4{};
      bool dynamic_rendering_local_read{};
      bool maintenance5{};
      bool maintenance6{};
      bool shader_draw_parameters{};
      bool vertex_pipeline_stores_and_atomics{};
   };

   [[nodiscard]] VpProfileProperties profile(const char *name, std::uint32_t version) {
      auto result = VpProfileProperties{};
      std::snprintf(result.profileName, VP_MAX_PROFILE_NAME_SIZE, "%s", name);
      result.specVersion = version;
      return result;
   }

   [[nodiscard]] std::vector<VpProfileProperties> rankedProfiles() {
      auto result = std::vector<VpProfileProperties>{};
#ifdef VP_KHR_ROADMAP_2026_NAME
      result.push_back(profile(VP_KHR_ROADMAP_2026_NAME, VP_KHR_ROADMAP_2026_SPEC_VERSION));
#endif
#ifdef VP_KHR_ROADMAP_2024_NAME
      result.push_back(profile(VP_KHR_ROADMAP_2024_NAME, VP_KHR_ROADMAP_2024_SPEC_VERSION));
#endif
#ifdef VP_KHR_ROADMAP_2022_NAME
      result.push_back(profile(VP_KHR_ROADMAP_2022_NAME, VP_KHR_ROADMAP_2022_SPEC_VERSION));
#endif
#ifdef VP_LUNARG_DESKTOP_BASELINE_2024_NAME
      result.push_back(profile(VP_LUNARG_DESKTOP_BASELINE_2024_NAME,
                               VP_LUNARG_DESKTOP_BASELINE_2024_SPEC_VERSION));
#endif
#ifdef VP_LUNARG_DESKTOP_BASELINE_2023_NAME
      result.push_back(profile(VP_LUNARG_DESKTOP_BASELINE_2023_NAME,
                               VP_LUNARG_DESKTOP_BASELINE_2023_SPEC_VERSION));
#endif
#ifdef VP_LUNARG_DESKTOP_BASELINE_2022_NAME
      result.push_back(profile(VP_LUNARG_DESKTOP_BASELINE_2022_NAME,
                               VP_LUNARG_DESKTOP_BASELINE_2022_SPEC_VERSION));
#endif
#ifdef VP_LUNARG_MINIMUM_REQUIREMENTS_1_3_NAME
      result.push_back(profile(VP_LUNARG_MINIMUM_REQUIREMENTS_1_3_NAME,
                               VP_LUNARG_MINIMUM_REQUIREMENTS_1_3_SPEC_VERSION));
#endif
#ifdef VP_LUNARG_MINIMUM_REQUIREMENTS_1_2_NAME
      result.push_back(profile(VP_LUNARG_MINIMUM_REQUIREMENTS_1_2_NAME,
                               VP_LUNARG_MINIMUM_REQUIREMENTS_1_2_SPEC_VERSION));
#endif
#ifdef VP_LUNARG_MINIMUM_REQUIREMENTS_1_1_NAME
      result.push_back(profile(VP_LUNARG_MINIMUM_REQUIREMENTS_1_1_NAME,
                               VP_LUNARG_MINIMUM_REQUIREMENTS_1_1_SPEC_VERSION));
#endif
#ifdef VP_LUNARG_MINIMUM_REQUIREMENTS_1_0_NAME
      result.push_back(profile(VP_LUNARG_MINIMUM_REQUIREMENTS_1_0_NAME,
                               VP_LUNARG_MINIMUM_REQUIREMENTS_1_0_SPEC_VERSION));
#endif
      return result;
   }

   [[nodiscard]] std::optional<SupportedProfile>
   highestProfile(vk::Instance instance, vk::PhysicalDevice device) {
      const auto profiles = rankedProfiles();
      for (std::uint32_t i{}; i < profiles.size(); ++i) {
         auto supported = VkBool32{};
         const auto result = vpGetPhysicalDeviceProfileSupport(
            static_cast<VkInstance>(instance), static_cast<VkPhysicalDevice>(device), &profiles[i], &supported);
         if (result == VK_SUCCESS && supported == VK_TRUE) {
            return SupportedProfile{.profile = profiles[i],
                                    .rank = static_cast<std::uint32_t>(profiles.size() - i)};
         }
      }
      return {};
   }

   [[nodiscard]] std::uint32_t typeRank(vk::PhysicalDeviceType type) {
      if (type == vk::PhysicalDeviceType::eDiscreteGpu) { return 3; }
      if (type == vk::PhysicalDeviceType::eIntegratedGpu) { return 2; }
      if (type == vk::PhysicalDeviceType::eVirtualGpu) { return 1; }
      return 0;
   }

   void appendUnique(std::vector<std::string> &names, std::string_view name) {
      const auto exists = std::ranges::any_of(names, [name](const std::string &known) { return known == name; });
      if (!exists) { names.emplace_back(name); }
   }

   void appendIfAvailable(std::vector<std::string> &names, std::span<const vk::ExtensionProperties> properties,
                          std::string_view name) {
      if (hasExtension(properties, name)) { appendUnique(names, name); }
   }

   [[nodiscard]] std::uint32_t preferredApiVersion(std::uint32_t supported) {
#ifdef VK_API_VERSION_1_4
      if (supported >= VK_API_VERSION_1_4) { return VK_API_VERSION_1_4; }
#endif
      if (supported >= VK_API_VERSION_1_3) { return VK_API_VERSION_1_3; }
      return supported;
   }

   [[nodiscard]] std::uint32_t apiRank(std::uint32_t version) {
#ifdef VK_API_VERSION_1_4
      if (version >= VK_API_VERSION_1_4) { return 2; }
#endif
      if (version >= VK_API_VERSION_1_3) { return 1; }
      return 0;
   }

   [[nodiscard]] ModernFeatures modernFeatures(vk::PhysicalDevice device,
                                               std::span<const vk::ExtensionProperties> extensions) {
      const auto has_descriptor_buffer =
#ifdef VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME
         hasExtension(extensions, VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME);
#else
         false;
#endif
      auto descriptor_buffer = vk::PhysicalDeviceDescriptorBufferFeaturesEXT{};
      auto features14 = vk::PhysicalDeviceVulkan14Features{};
      auto features13 = vk::PhysicalDeviceVulkan13Features{};
      auto features12 = vk::PhysicalDeviceVulkan12Features{};
      auto features11 = vk::PhysicalDeviceVulkan11Features{};
      auto features2 = vk::PhysicalDeviceFeatures2{};
      const auto api_version = device.getProperties().apiVersion;
      features2.pNext = &features11;
      features11.pNext = &features12;
      features12.pNext = &features13;
#ifdef VK_API_VERSION_1_4
      if (api_version >= VK_API_VERSION_1_4) {
         features13.pNext = &features14;
         if (has_descriptor_buffer) { features14.pNext = &descriptor_buffer; }
      } else
#endif
      if (has_descriptor_buffer) {
         features13.pNext = &descriptor_buffer;
      }
      device.getFeatures2(&features2);
      return ModernFeatures{.timeline_semaphore = features12.timelineSemaphore == VK_TRUE,
                            .synchronization2 = features13.synchronization2 == VK_TRUE,
                            .dynamic_rendering = features13.dynamicRendering == VK_TRUE,
                            .descriptor_indexing = features12.descriptorIndexing == VK_TRUE,
                            .sampled_image_update_after_bind =
                               features12.descriptorBindingSampledImageUpdateAfterBind == VK_TRUE,
                            .storage_buffer_update_after_bind =
                               features12.descriptorBindingStorageBufferUpdateAfterBind == VK_TRUE,
                            .partially_bound = features12.descriptorBindingPartiallyBound == VK_TRUE,
                            .variable_descriptor_count =
                               features12.descriptorBindingVariableDescriptorCount == VK_TRUE,
                            .runtime_descriptor_array = features12.runtimeDescriptorArray == VK_TRUE,
                            .descriptor_buffer = descriptor_buffer.descriptorBuffer == VK_TRUE,
                            .descriptor_buffer_image_layout_ignored =
                               descriptor_buffer.descriptorBufferImageLayoutIgnored == VK_TRUE,
                            .descriptor_buffer_push_descriptors =
                               descriptor_buffer.descriptorBufferPushDescriptors == VK_TRUE,
                            .maintenance4 = features13.maintenance4 == VK_TRUE,
                            .dynamic_rendering_local_read =
                               features14.dynamicRenderingLocalRead == VK_TRUE,
                            .maintenance5 = features14.maintenance5 == VK_TRUE,
                            .maintenance6 = features14.maintenance6 == VK_TRUE,
                            .shader_draw_parameters = features11.shaderDrawParameters == VK_TRUE,
                            .vertex_pipeline_stores_and_atomics =
                               features2.features.vertexPipelineStoresAndAtomics == VK_TRUE};
   }

   [[nodiscard]] bool hasRequiredModernFeatures(const ModernFeatures &features) {
      const auto indexed_sets = features.descriptor_indexing && features.sampled_image_update_after_bind &&
                                features.storage_buffer_update_after_bind && features.partially_bound &&
                                features.variable_descriptor_count && features.runtime_descriptor_array;
      return features.timeline_semaphore && features.synchronization2 && features.dynamic_rendering &&
             features.shader_draw_parameters && features.vertex_pipeline_stores_and_atomics &&
             (features.descriptor_buffer || indexed_sets);
   }

   [[nodiscard]] std::uint32_t featureRank(const ModernFeatures &features) {
      const auto optional = std::array{features.descriptor_buffer,
                                       features.descriptor_buffer_image_layout_ignored,
                                       features.descriptor_buffer_push_descriptors,
                                       features.descriptor_indexing,
                                       features.sampled_image_update_after_bind,
                                       features.storage_buffer_update_after_bind,
                                       features.partially_bound,
                                       features.variable_descriptor_count,
                                       features.runtime_descriptor_array,
                                       features.maintenance4,
                                       features.dynamic_rendering_local_read,
                                       features.maintenance5,
                                       features.maintenance6,
                                       features.shader_draw_parameters,
                                       features.vertex_pipeline_stores_and_atomics};
      return static_cast<std::uint32_t>(std::ranges::count(optional, true));
   }

   [[nodiscard]] std::uint32_t descriptorModelRank(const ModernFeatures &features) {
      if (features.descriptor_buffer) { return 2; }
      if (hasRequiredModernFeatures(features)) { return 1; }
      return 0;
   }

   void enableModernFeatures(const ModernFeatures &support, std::uint32_t api_version,
                             vk::PhysicalDeviceFeatures2 &features2,
                             vk::PhysicalDeviceVulkan11Features &features11,
                             vk::PhysicalDeviceVulkan12Features &features12,
                             vk::PhysicalDeviceVulkan13Features &features13,
                             vk::PhysicalDeviceVulkan14Features &features14,
                             vk::PhysicalDeviceDescriptorBufferFeaturesEXT &descriptor_buffer) {
      features2.pNext = &features11;
      features2.features.vertexPipelineStoresAndAtomics =
         support.vertex_pipeline_stores_and_atomics ? VK_TRUE : VK_FALSE;
      features11.pNext = &features12;
      features11.shaderDrawParameters = support.shader_draw_parameters ? VK_TRUE : VK_FALSE;
      features12.pNext = &features13;
      features12.timelineSemaphore = VK_TRUE;
      features12.descriptorIndexing = support.descriptor_indexing ? VK_TRUE : VK_FALSE;
      features12.descriptorBindingSampledImageUpdateAfterBind =
         support.sampled_image_update_after_bind ? VK_TRUE : VK_FALSE;
      features12.descriptorBindingStorageBufferUpdateAfterBind =
         support.storage_buffer_update_after_bind ? VK_TRUE : VK_FALSE;
      features12.descriptorBindingPartiallyBound = support.partially_bound ? VK_TRUE : VK_FALSE;
      features12.descriptorBindingVariableDescriptorCount = support.variable_descriptor_count ? VK_TRUE : VK_FALSE;
      features12.runtimeDescriptorArray = support.runtime_descriptor_array ? VK_TRUE : VK_FALSE;
      features13.synchronization2 = VK_TRUE;
      features13.dynamicRendering = VK_TRUE;
      features13.maintenance4 = support.maintenance4 ? VK_TRUE : VK_FALSE;
#ifdef VK_API_VERSION_1_4
      if (api_version >= VK_API_VERSION_1_4) {
         features13.pNext = &features14;
         features14.dynamicRenderingLocalRead = support.dynamic_rendering_local_read ? VK_TRUE : VK_FALSE;
         features14.maintenance5 = support.maintenance5 ? VK_TRUE : VK_FALSE;
         features14.maintenance6 = support.maintenance6 ? VK_TRUE : VK_FALSE;
         if (support.descriptor_buffer) { features14.pNext = &descriptor_buffer; }
      } else
#endif
      if (support.descriptor_buffer) {
         features13.pNext = &descriptor_buffer;
      }
      descriptor_buffer.descriptorBuffer = support.descriptor_buffer ? VK_TRUE : VK_FALSE;
      descriptor_buffer.descriptorBufferImageLayoutIgnored =
         support.descriptor_buffer_image_layout_ignored ? VK_TRUE : VK_FALSE;
      descriptor_buffer.descriptorBufferPushDescriptors =
         support.descriptor_buffer_push_descriptors ? VK_TRUE : VK_FALSE;
   }

   [[nodiscard]] vk::ImageSubresourceRange imageRange(vk::ImageAspectFlags aspect) {
      auto range = vk::ImageSubresourceRange{};
      range.aspectMask = aspect;
      range.levelCount = 1;
      range.layerCount = 1;
      return range;
   }

   [[nodiscard]] vk::ImageMemoryBarrier2
   imageBarrier(vk::Image image, vk::ImageAspectFlags aspect, vk::ImageLayout old_layout,
                vk::ImageLayout new_layout, vk::PipelineStageFlags2 src_stage,
                vk::AccessFlags2 src_access, vk::PipelineStageFlags2 dst_stage,
                vk::AccessFlags2 dst_access) {
      auto barrier = vk::ImageMemoryBarrier2{};
      barrier.srcStageMask = src_stage;
      barrier.srcAccessMask = src_access;
      barrier.dstStageMask = dst_stage;
      barrier.dstAccessMask = dst_access;
      barrier.oldLayout = old_layout;
      barrier.newLayout = new_layout;
      barrier.image = image;
      barrier.subresourceRange = imageRange(aspect);
      return barrier;
   }

   void imageBarrier(vk::CommandBuffer command_buffer, const vk::ImageMemoryBarrier2 &barrier) {
      auto dependency = vk::DependencyInfo{};
      dependency.imageMemoryBarrierCount = 1;
      dependency.pImageMemoryBarriers = &barrier;
      command_buffer.pipelineBarrier2(&dependency);
   }

   void bufferBarrier(vk::CommandBuffer command_buffer, const vk::BufferMemoryBarrier2 &barrier) {
      auto dependency = vk::DependencyInfo{};
      dependency.bufferMemoryBarrierCount = 1;
      dependency.pBufferMemoryBarriers = &barrier;
      command_buffer.pipelineBarrier2(&dependency);
   }

   [[nodiscard]] vk::BufferMemoryBarrier2
   bufferBarrier(vk::Buffer buffer, vk::DeviceSize size, vk::PipelineStageFlags2 src_stage,
                 vk::AccessFlags2 src_access, vk::PipelineStageFlags2 dst_stage, vk::AccessFlags2 dst_access) {
      auto barrier = vk::BufferMemoryBarrier2{};
      barrier.srcStageMask = src_stage;
      barrier.srcAccessMask = src_access;
      barrier.dstStageMask = dst_stage;
      barrier.dstAccessMask = dst_access;
      barrier.buffer = buffer;
      barrier.offset = 0;
      barrier.size = size;
      return barrier;
   }

   [[nodiscard]] vk::Result beginOneTime(vk::Device device, vk::CommandPool pool,
                                         vk::CommandBuffer command_buffer) {
      auto result = device.resetCommandPool(pool);
      if (result != vk::Result::eSuccess) { return result; }
      auto begin = vk::CommandBufferBeginInfo{vk::CommandBufferUsageFlagBits::eOneTimeSubmit};
      return command_buffer.begin(&begin);
   }

   [[nodiscard]] vk::RenderingAttachmentInfo colorAttachment(vk::ImageView view,
                                                             const vk::ClearColorValue &clear_color) {
      auto clear = vk::ClearValue{};
      clear.color = clear_color;
      auto attachment = vk::RenderingAttachmentInfo{};
      attachment.imageView = view;
      attachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
      attachment.loadOp = vk::AttachmentLoadOp::eClear;
      attachment.storeOp = vk::AttachmentStoreOp::eStore;
      attachment.clearValue = clear;
      return attachment;
   }

   [[nodiscard]] vk::RenderingAttachmentInfo depthAttachment(vk::ImageView view) {
      auto clear = vk::ClearValue{};
      clear.depthStencil = vk::ClearDepthStencilValue{1.0F, 0};
      auto attachment = vk::RenderingAttachmentInfo{};
      attachment.imageView = view;
      attachment.imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
      attachment.loadOp = vk::AttachmentLoadOp::eClear;
      attachment.storeOp = vk::AttachmentStoreOp::eStore;
      attachment.clearValue = clear;
      return attachment;
   }

   void setViewportAndScissor(vk::CommandBuffer command_buffer, vk::Extent2D extent) {
      auto viewport = vk::Viewport{0.0F, 0.0F, static_cast<float>(extent.width), static_cast<float>(extent.height),
                                   0.0F, 1.0F};
      auto scissor = vk::Rect2D{vk::Offset2D{0, 0}, extent};
      command_buffer.setViewport(0, 1, &viewport);
      command_buffer.setScissor(0, 1, &scissor);
   }

   [[nodiscard]] std::vector<std::string>
   profileDeviceExtensions(const VpProfileProperties &profile,
                           std::span<const vk::ExtensionProperties> available) {
      std::uint32_t count{};
      if (vpGetProfileDeviceExtensionProperties(&profile, nullptr, &count, nullptr) != VK_SUCCESS) { return {}; }

      auto extensions = std::vector<VkExtensionProperties>(count);
      if (vpGetProfileDeviceExtensionProperties(&profile, nullptr, &count, extensions.data()) != VK_SUCCESS) {
         return {};
      }

      auto result = std::vector<std::string>{};
      for (const auto &extension : extensions) {
         if (hasExtension(available, extension.extensionName)) { appendUnique(result, extension.extensionName); }
      }
      return result;
   }

   [[nodiscard]] std::vector<std::string>
   profileInstanceExtensions(std::span<const vk::ExtensionProperties> available) {
      auto result = std::vector<std::string>{};
      for (const auto &profile : rankedProfiles()) {
         std::uint32_t count{};
         if (vpGetProfileInstanceExtensionProperties(&profile, nullptr, &count, nullptr) != VK_SUCCESS) { continue; }

         auto extensions = std::vector<VkExtensionProperties>(count);
         if (vpGetProfileInstanceExtensionProperties(&profile, nullptr, &count, extensions.data()) != VK_SUCCESS) {
            continue;
         }
         for (const auto &extension : extensions) {
            if (hasExtension(available, extension.extensionName)) { appendUnique(result, extension.extensionName); }
         }
      }
      return result;
   }

} // namespace vve::v4::vh::low::detail

namespace vve::v4::vh::low {

   /// @brief Checks whether a Vulkan extension list contains one named extension.
   bool hasExtension(std::span<const vk::ExtensionProperties> properties, std::string_view name) {
      return std::ranges::any_of(properties, [name](const vk::ExtensionProperties &property) {
         return std::string_view{property.extensionName.data()} == name;
      });
   }

   /// @brief Enumerates Vulkan instance extensions without storing state.
   std::vector<vk::ExtensionProperties> instanceExtensions() {
      std::uint32_t count{};
      if (vk::enumerateInstanceExtensionProperties(nullptr, &count, nullptr) != vk::Result::eSuccess) { return {}; }
      auto result = std::vector<vk::ExtensionProperties>(count);
      if (vk::enumerateInstanceExtensionProperties(nullptr, &count, result.data()) != vk::Result::eSuccess) {
         return {};
      }
      return result;
   }

   /// @brief Enumerates Vulkan device extensions without storing state.
   std::vector<vk::ExtensionProperties> deviceExtensions(vk::PhysicalDevice device) {
      std::uint32_t count{};
      if (device.enumerateDeviceExtensionProperties(nullptr, &count, nullptr) != vk::Result::eSuccess) { return {}; }
      auto result = std::vector<vk::ExtensionProperties>(count);
      if (device.enumerateDeviceExtensionProperties(nullptr, &count, result.data()) != vk::Result::eSuccess) {
         return {};
      }
      return result;
   }

   /// @brief Chooses a predictable SRGB surface format when available.
   vk::SurfaceFormatKHR chooseSurfaceFormat(std::span<const vk::SurfaceFormatKHR> formats) {
      const auto wanted = [](const vk::SurfaceFormatKHR &format) {
         return format.format == vk::Format::eB8G8R8A8Srgb &&
                format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
      };
      const auto it = std::ranges::find_if(formats, wanted);
      return it == formats.end() ? formats.front() : *it;
   }

   /// @brief Chooses a swapchain extent from surface limits and a caller fallback.
   vk::Extent2D chooseExtent(const vk::SurfaceCapabilitiesKHR &caps, vk::Extent2D fallback) {
      if (caps.currentExtent.width != std::numeric_limits<std::uint32_t>::max()) { return caps.currentExtent; }
      const auto clamp = [](std::uint32_t value, std::uint32_t low, std::uint32_t high) {
         return std::min(std::max(value, low), high);
      };
      return vk::Extent2D{clamp(fallback.width, caps.minImageExtent.width, caps.maxImageExtent.width),
                          clamp(fallback.height, caps.minImageExtent.height, caps.maxImageExtent.height)};
   }

   /// @brief Chooses the first supported composite-alpha mode from a stable preference list.
   vk::CompositeAlphaFlagBitsKHR chooseCompositeAlpha(vk::CompositeAlphaFlagsKHR supported) {
      constexpr auto choices = std::array{vk::CompositeAlphaFlagBitsKHR::eOpaque,
                                          vk::CompositeAlphaFlagBitsKHR::ePreMultiplied,
                                          vk::CompositeAlphaFlagBitsKHR::ePostMultiplied,
                                          vk::CompositeAlphaFlagBitsKHR::eInherit};
      const auto it = std::ranges::find_if(choices, [supported](auto choice) {
         return (supported & choice) == choice;
      });
      return it == choices.end() ? vk::CompositeAlphaFlagBitsKHR::eOpaque : *it;
   }

   /// @brief Finds a memory type matching Vulkan memory requirements and requested properties.
   std::optional<std::uint32_t>
   findMemoryType(vk::PhysicalDevice device, std::uint32_t bits, vk::MemoryPropertyFlags flags) {
      const auto props = device.getMemoryProperties();
      for (std::uint32_t i{}; i < props.memoryTypeCount; ++i) {
         if ((bits & (1U << i)) != 0 && (props.memoryTypes[i].propertyFlags & flags) == flags) { return i; }
      }
      return {};
   }

   /// @brief Creates a Vulkan instance from caller-provided platform extensions plus portability extensions.
   vk::Result createInstance(std::string_view app_name, std::span<const char *const> required_extensions,
                             vk::Instance *instance) {
      auto extensions = std::vector<std::string>{};
      for (const auto *extension : required_extensions) { detail::appendUnique(extensions, extension); }
      auto flags = vk::InstanceCreateFlags{};
      const auto available = instanceExtensions();
      std::uint32_t api_version = VK_API_VERSION_1_0;
      if (vk::enumerateInstanceVersion(&api_version) != vk::Result::eSuccess) { api_version = VK_API_VERSION_1_0; }
      api_version = detail::preferredApiVersion(api_version);
      for (const auto &extension : detail::profileInstanceExtensions(available)) {
         detail::appendUnique(extensions, extension);
      }
#ifdef VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
      if (hasExtension(available, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME)) {
         detail::appendUnique(extensions, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
      }
#endif
#ifdef VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
      if (hasExtension(available, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)) {
         detail::appendUnique(extensions, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
         flags |= vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR;
      }
#endif
      auto extension_names = std::vector<const char *>{};
      extension_names.reserve(extensions.size());
      for (const auto &extension : extensions) { extension_names.push_back(extension.c_str()); }

      auto app = vk::ApplicationInfo{};
      app.pApplicationName = app_name.data();
      app.apiVersion = api_version;
      auto info = vk::InstanceCreateInfo{};
      info.flags = flags;
      info.pApplicationInfo = &app;
      info.enabledExtensionCount = static_cast<std::uint32_t>(extension_names.size());
      info.ppEnabledExtensionNames = extension_names.data();
      return vk::createInstance(&info, nullptr, instance);
   }

   /// @brief Selects a physical device and graphics/present queue for all provided surfaces.
   bool chooseDevice(vk::Instance instance, std::span<const vk::SurfaceKHR> surfaces, vk::PhysicalDevice *device,
                     std::uint32_t *queue_family, std::vector<std::string> *extensions) {
      std::uint32_t device_count{};
      if (instance.enumeratePhysicalDevices(&device_count, nullptr) != vk::Result::eSuccess || device_count == 0) {
         return false;
      }

      auto devices = std::vector<vk::PhysicalDevice>(device_count);
      if (instance.enumeratePhysicalDevices(&device_count, devices.data()) != vk::Result::eSuccess) {
         return false;
      }
      struct Candidate {
         vk::PhysicalDevice device{};
         std::uint32_t queue_family{};
         std::uint32_t type_rank{};
         std::uint32_t api_rank{};
         std::uint32_t profile_rank{};
         std::uint32_t descriptor_rank{};
         std::uint32_t feature_rank{};
         std::vector<std::string> extensions{};
      };
      auto best = std::optional<Candidate>{};
      for (const auto candidate : devices) {
         const auto available = deviceExtensions(candidate);
         if (!hasExtension(available, VK_KHR_SWAPCHAIN_EXTENSION_NAME)) { continue; }
         const auto properties = candidate.getProperties();
         if (detail::apiRank(properties.apiVersion) == 0) { continue; }
         const auto modern_features = detail::modernFeatures(candidate, available);
         if (!detail::hasRequiredModernFeatures(modern_features)) { continue; }
         const auto supported_profile = detail::highestProfile(instance, candidate);

         auto queues = candidate.getQueueFamilyProperties();
         for (std::uint32_t i{}; i < queues.size(); ++i) {
            if ((queues[i].queueFlags & vk::QueueFlagBits::eGraphics) != vk::QueueFlagBits::eGraphics) { continue; }
            const auto presents = std::ranges::all_of(surfaces, [&](vk::SurfaceKHR surface) {
               vk::Bool32 supported{};
               return candidate.getSurfaceSupportKHR(i, surface, &supported) == vk::Result::eSuccess &&
                      supported == VK_TRUE;
            });
            if (!presents) { continue; }

            auto enabled = supported_profile ? detail::profileDeviceExtensions(supported_profile->profile, available)
                                             : std::vector<std::string>{};
            detail::appendUnique(enabled, VK_KHR_SWAPCHAIN_EXTENSION_NAME);
#ifdef VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME
            if (modern_features.descriptor_buffer) {
               detail::appendUnique(enabled, VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME);
            }
#endif
            detail::appendIfAvailable(enabled, available, "VK_KHR_portability_subset");
            const auto option = Candidate{.device = candidate,
                                          .queue_family = i,
                                          .type_rank = detail::typeRank(properties.deviceType),
                                          .api_rank = detail::apiRank(properties.apiVersion),
                                          .profile_rank = supported_profile ? supported_profile->rank : 0U,
                                          .descriptor_rank = detail::descriptorModelRank(modern_features),
                                          .feature_rank = detail::featureRank(modern_features),
                                          .extensions = std::move(enabled)};
            if (!best ||
                std::tie(option.type_rank, option.api_rank, option.profile_rank, option.descriptor_rank,
                         option.feature_rank) >
                   std::tie(best->type_rank, best->api_rank, best->profile_rank, best->descriptor_rank,
                            best->feature_rank)) {
               best = option;
            }
         }
      }
      if (!best) { return false; }

      *device = best->device;
      *queue_family = best->queue_family;
      *extensions = std::move(best->extensions);
      return true;
   }

   /// @brief Creates a logical device and retrieves one graphics/present queue.
   vk::Result createDevice(vk::PhysicalDevice physical_device, std::uint32_t queue_family,
                           std::span<const std::string> extensions, vk::Device *device, vk::Queue *queue) {
      const auto api_version = physical_device.getProperties().apiVersion;
      const auto available = deviceExtensions(physical_device);
      const auto support = detail::modernFeatures(physical_device, available);
      if (detail::apiRank(api_version) == 0 || !detail::hasRequiredModernFeatures(support)) {
         return vk::Result::eErrorFeatureNotPresent;
      }
      auto enabled = std::vector<std::string>(extensions.begin(), extensions.end());
#ifdef VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME
      if (support.descriptor_buffer) { detail::appendUnique(enabled, VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME); }
#endif
      detail::appendIfAvailable(enabled, available, "VK_KHR_portability_subset");
      auto extension_names = std::vector<const char *>{};
      extension_names.reserve(enabled.size());
      for (const auto &extension : enabled) { extension_names.push_back(extension.c_str()); }

      auto features14 = vk::PhysicalDeviceVulkan14Features{};
      auto features13 = vk::PhysicalDeviceVulkan13Features{};
      auto features12 = vk::PhysicalDeviceVulkan12Features{};
      auto features11 = vk::PhysicalDeviceVulkan11Features{};
      auto descriptor_buffer = vk::PhysicalDeviceDescriptorBufferFeaturesEXT{};
      auto features2 = vk::PhysicalDeviceFeatures2{};
      detail::enableModernFeatures(support, api_version, features2, features11, features12, features13, features14,
                                   descriptor_buffer);

      auto priority = 1.0F;
      auto queue_info = vk::DeviceQueueCreateInfo{};
      queue_info.queueFamilyIndex = queue_family;
      queue_info.queueCount = 1;
      queue_info.pQueuePriorities = &priority;
      auto info = vk::DeviceCreateInfo{};
      info.pNext = &features2;
      info.queueCreateInfoCount = 1;
      info.pQueueCreateInfos = &queue_info;
      info.enabledExtensionCount = static_cast<std::uint32_t>(extension_names.size());
      info.ppEnabledExtensionNames = extension_names.data();
      const auto result = physical_device.createDevice(&info, nullptr, device);
      if (result == vk::Result::eSuccess) { *queue = device->getQueue(queue_family, 0); }
      return result;
   }

   /// @brief Creates a FIFO swapchain and one color image view per swapchain image.
   vk::Result createSwapchainAndViews(vk::PhysicalDevice physical_device, vk::Device device, vk::SurfaceKHR surface,
                                      vk::Extent2D fallback_extent, vk::SwapchainKHR *swapchain,
                                      vk::Format *color_format, vk::Extent2D *extent,
                                      std::vector<vk::Image> *images,
                                      std::vector<vk::ImageView> *image_views) {
      vk::SurfaceCapabilitiesKHR caps{};
      auto result = physical_device.getSurfaceCapabilitiesKHR(surface, &caps);
      if (result != vk::Result::eSuccess) { return result; }

      std::uint32_t format_count{};
      result = physical_device.getSurfaceFormatsKHR(surface, &format_count, nullptr);
      if (result != vk::Result::eSuccess) { return result; }
      auto formats = std::vector<vk::SurfaceFormatKHR>(format_count);
      result = physical_device.getSurfaceFormatsKHR(surface, &format_count, formats.data());
      if (result != vk::Result::eSuccess) { return result; }
      if (formats.empty()) { return vk::Result::eErrorFormatNotSupported; }

      const auto format = chooseSurfaceFormat(formats);
      auto image_count = caps.minImageCount + 1;
      if (caps.maxImageCount > 0) { image_count = std::min(image_count, caps.maxImageCount); }
      *extent = chooseExtent(caps, fallback_extent);
      *color_format = format.format;

      auto info = vk::SwapchainCreateInfoKHR{};
      info.surface = surface;
      info.minImageCount = image_count;
      info.imageFormat = format.format;
      info.imageColorSpace = format.colorSpace;
      info.imageExtent = *extent;
      info.imageArrayLayers = 1;
      info.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
      info.imageSharingMode = vk::SharingMode::eExclusive;
      info.preTransform = caps.currentTransform;
      info.compositeAlpha = chooseCompositeAlpha(caps.supportedCompositeAlpha);
      info.presentMode = vk::PresentModeKHR::eFifo;
      info.clipped = VK_TRUE;
      result = device.createSwapchainKHR(&info, nullptr, swapchain);
      if (result != vk::Result::eSuccess) { return result; }

      std::uint32_t count{};
      result = device.getSwapchainImagesKHR(*swapchain, &count, nullptr);
      if (result != vk::Result::eSuccess) { return result; }
      images->resize(count);
      result = device.getSwapchainImagesKHR(*swapchain, &count, images->data());
      if (result != vk::Result::eSuccess) { return result; }

      image_views->clear();
      image_views->reserve(images->size());
      for (const auto image : *images) {
         auto range = vk::ImageSubresourceRange{};
         range.aspectMask = vk::ImageAspectFlagBits::eColor;
         range.levelCount = 1;
         range.layerCount = 1;
         auto view_info = vk::ImageViewCreateInfo{};
         view_info.image = image;
         view_info.viewType = vk::ImageViewType::e2D;
         view_info.format = format.format;
         view_info.subresourceRange = range;
         vk::ImageView view{};
         result = device.createImageView(&view_info, nullptr, &view);
         if (result != vk::Result::eSuccess) { return result; }
         image_views->push_back(view);
      }
      return vk::Result::eSuccess;
   }

   /// @brief Creates a device-local image, its memory, and a matching 2D image view.
   vk::Result createImageTarget(vk::PhysicalDevice physical_device, vk::Device device, vk::Extent2D extent,
                                vk::Format format, vk::ImageUsageFlags usage, vk::FormatFeatureFlags features,
                                vk::ImageAspectFlags aspect, vk::Image *image, vk::DeviceMemory *memory,
                                vk::ImageView *view) {
      const auto props = physical_device.getFormatProperties(format);
      if ((props.optimalTilingFeatures & features) != features) { return vk::Result::eErrorFormatNotSupported; }

      auto image_info = vk::ImageCreateInfo{};
      image_info.imageType = vk::ImageType::e2D;
      image_info.format = format;
      image_info.extent = vk::Extent3D{extent.width, extent.height, 1};
      image_info.mipLevels = 1;
      image_info.arrayLayers = 1;
      image_info.samples = vk::SampleCountFlagBits::e1;
      image_info.tiling = vk::ImageTiling::eOptimal;
      image_info.usage = usage;
      auto result = device.createImage(&image_info, nullptr, image);
      if (result != vk::Result::eSuccess) { return result; }

      const auto requirements = device.getImageMemoryRequirements(*image);
      const auto memory_type = findMemoryType(physical_device, requirements.memoryTypeBits,
                                              vk::MemoryPropertyFlagBits::eDeviceLocal);
      if (!memory_type) { return vk::Result::eErrorMemoryMapFailed; }

      auto memory_info = vk::MemoryAllocateInfo{};
      memory_info.allocationSize = requirements.size;
      memory_info.memoryTypeIndex = *memory_type;
      result = device.allocateMemory(&memory_info, nullptr, memory);
      if (result != vk::Result::eSuccess) { return result; }
      result = device.bindImageMemory(*image, *memory, 0);
      if (result != vk::Result::eSuccess) { return result; }

      auto range = vk::ImageSubresourceRange{};
      range.aspectMask = aspect;
      range.levelCount = 1;
      range.layerCount = 1;
      auto view_info = vk::ImageViewCreateInfo{};
      view_info.image = *image;
      view_info.viewType = vk::ImageViewType::e2D;
      view_info.format = format;
      view_info.subresourceRange = range;
      return device.createImageView(&view_info, nullptr, view);
   }

   /// @brief Creates a device-local depth image, allocates memory, binds it, and creates its view.
   vk::Result createDepthTarget(vk::PhysicalDevice physical_device, vk::Device device, vk::Extent2D extent,
                                vk::Format *format, vk::Image *image, vk::DeviceMemory *memory,
                                vk::ImageView *view) {
      constexpr auto formats = std::array{vk::Format::eD32Sfloat, vk::Format::eD24UnormS8Uint,
                                          vk::Format::eD32SfloatS8Uint};
      const auto features = vk::FormatFeatureFlagBits::eDepthStencilAttachment;
      const auto it = std::ranges::find_if(formats, [&](vk::Format candidate) {
         return (physical_device.getFormatProperties(candidate).optimalTilingFeatures & features) == features;
      });
      if (it == formats.end()) { return vk::Result::eErrorFormatNotSupported; }

      *format = *it;
      return createImageTarget(physical_device, device, extent, *format,
                               vk::ImageUsageFlagBits::eDepthStencilAttachment, features,
                               vk::ImageAspectFlagBits::eDepth, image, memory, view);
   }

   /// @brief Creates a readable D32 shadow-map depth image and its view.
   vk::Result createShadowDepthTarget(vk::PhysicalDevice physical_device, vk::Device device, vk::Extent2D extent,
                                      vk::Image *image, vk::DeviceMemory *memory, vk::ImageView *view) {
      const auto features = vk::FormatFeatureFlagBits::eDepthStencilAttachment;
      const auto usage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eTransferSrc |
                         vk::ImageUsageFlagBits::eSampled;
      return createImageTarget(physical_device, device, extent, vk::Format::eD32Sfloat, usage, features,
                               vk::ImageAspectFlagBits::eDepth, image, memory, view);
   }

   /// @brief Creates the nearest sampler used for exact shadow-depth verification.
   vk::Result createShadowSampler(vk::Device device, vk::Sampler *sampler) {
      auto info = vk::SamplerCreateInfo{};
      info.magFilter = vk::Filter::eNearest;
      info.minFilter = vk::Filter::eNearest;
      info.mipmapMode = vk::SamplerMipmapMode::eNearest;
      info.addressModeU = vk::SamplerAddressMode::eClampToEdge;
      info.addressModeV = vk::SamplerAddressMode::eClampToEdge;
      info.addressModeW = vk::SamplerAddressMode::eClampToEdge;
      info.maxLod = 1.0F;
      return device.createSampler(&info, nullptr, sampler);
   }

   /// @brief Creates reusable command and timeline objects for one serial frame executor.
   vk::Result createFrameExecutor(vk::Device device, std::uint32_t queue_family, vk::CommandPool *pool,
                                  vk::CommandBuffer *command_buffer, vk::Semaphore *timeline,
                                  vk::Fence *acquire_fence) {
      auto pool_info = vk::CommandPoolCreateInfo{};
      pool_info.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
      pool_info.queueFamilyIndex = queue_family;
      auto result = device.createCommandPool(&pool_info, nullptr, pool);
      if (result != vk::Result::eSuccess) { return result; }

      auto buffer_info = vk::CommandBufferAllocateInfo{};
      buffer_info.commandPool = *pool;
      buffer_info.level = vk::CommandBufferLevel::ePrimary;
      buffer_info.commandBufferCount = 1;
      result = device.allocateCommandBuffers(&buffer_info, command_buffer);
      if (result != vk::Result::eSuccess) { return result; }

      auto timeline_info = vk::SemaphoreTypeCreateInfo{};
      timeline_info.semaphoreType = vk::SemaphoreType::eTimeline;
      timeline_info.initialValue = 0;
      auto semaphore_info = vk::SemaphoreCreateInfo{};
      semaphore_info.pNext = &timeline_info;
      result = device.createSemaphore(&semaphore_info, nullptr, timeline);
      if (result != vk::Result::eSuccess) { return result; }

      auto fence_info = vk::FenceCreateInfo{};
      return device.createFence(&fence_info, nullptr, acquire_fence);
   }

   /// @brief Creates one host-visible buffer for the first direct CPU upload path.
   vk::Result createHostBuffer(vk::PhysicalDevice physical_device, vk::Device device, vk::DeviceSize size,
                               vk::BufferUsageFlags usage, vk::Buffer *buffer, vk::DeviceMemory *memory) {
      if (size == 0) { return vk::Result::eErrorInitializationFailed; }

      auto info = vk::BufferCreateInfo{};
      info.size = size;
      info.usage = usage;
      info.sharingMode = vk::SharingMode::eExclusive;
      auto result = device.createBuffer(&info, nullptr, buffer);
      if (result != vk::Result::eSuccess) { return result; }

      const auto requirements = device.getBufferMemoryRequirements(*buffer);
      const auto flags = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
      const auto memory_type = findMemoryType(physical_device, requirements.memoryTypeBits, flags);
      if (!memory_type) {
         device.destroyBuffer(*buffer);
         *buffer = nullptr;
         return vk::Result::eErrorMemoryMapFailed;
      }

      auto memory_info = vk::MemoryAllocateInfo{};
      memory_info.allocationSize = requirements.size;
      memory_info.memoryTypeIndex = *memory_type;
      result = device.allocateMemory(&memory_info, nullptr, memory);
      if (result != vk::Result::eSuccess) {
         device.destroyBuffer(*buffer);
         *buffer = nullptr;
         return result;
      }

      result = device.bindBufferMemory(*buffer, *memory, 0);
      if (result != vk::Result::eSuccess) {
         device.freeMemory(*memory);
         device.destroyBuffer(*buffer);
         *memory = nullptr;
         *buffer = nullptr;
      }
      return result;
   }

   /// @brief Writes bytes into a host-visible coherent Vulkan buffer allocation.
   vk::Result writeBuffer(vk::Device device, vk::DeviceMemory memory, std::span<const std::byte> bytes) {
      void *mapped{};
      auto result = device.mapMemory(memory, 0, bytes.size(), {}, &mapped);
      if (result != vk::Result::eSuccess) { return result; }
      std::ranges::copy(bytes, static_cast<std::byte *>(mapped));
      device.unmapMemory(memory);
      return vk::Result::eSuccess;
   }

   /// @brief Reads bytes from a host-visible coherent Vulkan buffer allocation.
   vk::Result readBuffer(vk::Device device, vk::DeviceMemory memory, std::span<std::byte> bytes) {
      void *mapped{};
      auto result = device.mapMemory(memory, 0, bytes.size(), {}, &mapped);
      if (result != vk::Result::eSuccess) { return result; }
      std::ranges::copy(std::span{static_cast<std::byte *>(mapped), bytes.size()}, bytes.begin());
      device.unmapMemory(memory);
      return vk::Result::eSuccess;
   }

   /// @brief Creates the descriptor set for scene debug output and shadow-map sampling.
   vk::Result createSceneDescriptor(vk::Device device, vk::Buffer debug_buffer, vk::DeviceSize debug_size,
                                    vk::ImageView shadow_view, vk::Sampler shadow_sampler,
                                    vk::DescriptorSetLayout *layout, vk::DescriptorPool *pool,
                                    vk::DescriptorSet *set) {
      constexpr auto shader_stages = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
      auto bindings = std::array{vk::DescriptorSetLayoutBinding{0, vk::DescriptorType::eStorageBuffer, 1,
                                                                shader_stages},
                                 vk::DescriptorSetLayoutBinding{1, vk::DescriptorType::eSampledImage, 1,
                                                                shader_stages},
                                 vk::DescriptorSetLayoutBinding{2, vk::DescriptorType::eSampler, 1,
                                                                shader_stages}};
      auto layout_info = vk::DescriptorSetLayoutCreateInfo{};
      layout_info.bindingCount = static_cast<std::uint32_t>(bindings.size());
      layout_info.pBindings = bindings.data();
      auto result = device.createDescriptorSetLayout(&layout_info, nullptr, layout);
      if (result != vk::Result::eSuccess) { return result; }

      auto pool_sizes = std::array{vk::DescriptorPoolSize{vk::DescriptorType::eStorageBuffer, 1},
                                   vk::DescriptorPoolSize{vk::DescriptorType::eSampledImage, 1},
                                   vk::DescriptorPoolSize{vk::DescriptorType::eSampler, 1}};
      auto pool_info = vk::DescriptorPoolCreateInfo{};
      pool_info.maxSets = 1;
      pool_info.poolSizeCount = static_cast<std::uint32_t>(pool_sizes.size());
      pool_info.pPoolSizes = pool_sizes.data();
      result = device.createDescriptorPool(&pool_info, nullptr, pool);
      if (result != vk::Result::eSuccess) {
         device.destroyDescriptorSetLayout(*layout);
         *layout = nullptr;
         return result;
      }

      auto allocate_info = vk::DescriptorSetAllocateInfo{};
      allocate_info.descriptorPool = *pool;
      allocate_info.descriptorSetCount = 1;
      allocate_info.pSetLayouts = layout;
      result = device.allocateDescriptorSets(&allocate_info, set);
      if (result != vk::Result::eSuccess) { return result; }

      auto buffer_info = vk::DescriptorBufferInfo{debug_buffer, 0, debug_size};
      auto image_info = vk::DescriptorImageInfo{{}, shadow_view, vk::ImageLayout::eShaderReadOnlyOptimal};
      auto sampler_info = vk::DescriptorImageInfo{shadow_sampler, {}, vk::ImageLayout::eUndefined};
      auto writes = std::array{vk::WriteDescriptorSet{*set, 0, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr,
                                                      &buffer_info},
                               vk::WriteDescriptorSet{*set, 1, 0, 1, vk::DescriptorType::eSampledImage,
                                                      &image_info},
                               vk::WriteDescriptorSet{*set, 2, 0, 1, vk::DescriptorType::eSampler,
                                                      &sampler_info}};
      device.updateDescriptorSets(static_cast<std::uint32_t>(writes.size()), writes.data(), 0, nullptr);
      return vk::Result::eSuccess;
   }

   /// @brief Records a dynamic-rendering pass that clears one acquired swapchain image.
   vk::Result recordSwapchainClear(vk::Device device, vk::CommandPool pool, vk::CommandBuffer command_buffer,
                                   vk::Image image, vk::ImageView view, vk::Extent2D extent,
                                   vk::ImageLayout old_layout, const vk::ClearColorValue &clear_color) {
      auto result = detail::beginOneTime(device, pool, command_buffer);
      if (result != vk::Result::eSuccess) { return result; }

      auto to_attachment = detail::imageBarrier(image, vk::ImageAspectFlagBits::eColor, old_layout,
                                                vk::ImageLayout::eColorAttachmentOptimal,
                                                vk::PipelineStageFlagBits2::eNone, vk::AccessFlagBits2::eNone,
                                                vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                                                vk::AccessFlagBits2::eColorAttachmentWrite);
      detail::imageBarrier(command_buffer, to_attachment);

      auto attachment = detail::colorAttachment(view, clear_color);
      auto rendering = vk::RenderingInfo{};
      rendering.renderArea = vk::Rect2D{vk::Offset2D{0, 0}, extent};
      rendering.layerCount = 1;
      rendering.colorAttachmentCount = 1;
      rendering.pColorAttachments = &attachment;
      command_buffer.beginRendering(&rendering);
      command_buffer.endRendering();

      auto to_present = to_attachment;
      to_present.srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
      to_present.srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite;
      to_present.dstStageMask = vk::PipelineStageFlagBits2::eNone;
      to_present.dstAccessMask = vk::AccessFlagBits2::eNone;
      to_present.oldLayout = vk::ImageLayout::eColorAttachmentOptimal;
      to_present.newLayout = vk::ImageLayout::ePresentSrcKHR;
      detail::imageBarrier(command_buffer, to_present);
      return command_buffer.end();
   }

   /// @brief Creates one dynamic-rendering graphics pipeline from explicit Vulkan descriptions.
   vk::Result createGraphicsPipeline(vk::Device device, std::span<const std::uint32_t> vertex_spirv,
                                     std::string_view vertex_entry,
                                     std::span<const std::uint32_t> fragment_spirv,
                                     std::string_view fragment_entry,
                                     std::span<const vk::DescriptorSetLayout> set_layouts,
                                     std::span<const vk::PushConstantRange> push_ranges,
                                     std::span<const vk::VertexInputBindingDescription> vertex_bindings,
                                     std::span<const vk::VertexInputAttributeDescription> vertex_attributes,
                                     std::span<const vk::Format> color_formats, vk::Format depth_format,
                                     bool depth_enabled, vk::PipelineLayout *layout, vk::Pipeline *pipeline) {
      auto shader_info = vk::ShaderModuleCreateInfo{};
      shader_info.codeSize = vertex_spirv.size_bytes();
      shader_info.pCode = vertex_spirv.data();
      vk::ShaderModule vertex_shader{};
      auto result = device.createShaderModule(&shader_info, nullptr, &vertex_shader);
      if (result != vk::Result::eSuccess) { return result; }

      vk::ShaderModule fragment_shader{};
      if (!fragment_spirv.empty()) {
         shader_info.codeSize = fragment_spirv.size_bytes();
         shader_info.pCode = fragment_spirv.data();
         result = device.createShaderModule(&shader_info, nullptr, &fragment_shader);
         if (result != vk::Result::eSuccess) {
            device.destroyShaderModule(vertex_shader);
            return result;
         }
      }

      auto layout_info = vk::PipelineLayoutCreateInfo{};
      layout_info.setLayoutCount = static_cast<std::uint32_t>(set_layouts.size());
      layout_info.pSetLayouts = set_layouts.data();
      layout_info.pushConstantRangeCount = static_cast<std::uint32_t>(push_ranges.size());
      layout_info.pPushConstantRanges = push_ranges.data();
      result = device.createPipelineLayout(&layout_info, nullptr, layout);
      if (result != vk::Result::eSuccess) {
         if (fragment_shader) { device.destroyShaderModule(fragment_shader); }
         device.destroyShaderModule(vertex_shader);
         return result;
      }

      const auto vertex_name = std::string{vertex_entry};
      const auto fragment_name = std::string{fragment_entry};
      auto stages = std::array{vk::PipelineShaderStageCreateInfo{}, vk::PipelineShaderStageCreateInfo{}};
      stages[0].stage = vk::ShaderStageFlagBits::eVertex;
      stages[0].module = vertex_shader;
      stages[0].pName = vertex_name.c_str();
      auto vertex_input = vk::PipelineVertexInputStateCreateInfo{};
      vertex_input.vertexBindingDescriptionCount = static_cast<std::uint32_t>(vertex_bindings.size());
      vertex_input.pVertexBindingDescriptions = vertex_bindings.data();
      vertex_input.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(vertex_attributes.size());
      vertex_input.pVertexAttributeDescriptions = vertex_attributes.data();

      auto assembly = vk::PipelineInputAssemblyStateCreateInfo{};
      assembly.topology = vk::PrimitiveTopology::eTriangleList;
      auto viewport_state = vk::PipelineViewportStateCreateInfo{};
      viewport_state.viewportCount = 1;
      viewport_state.scissorCount = 1;
      auto raster = vk::PipelineRasterizationStateCreateInfo{};
      raster.polygonMode = vk::PolygonMode::eFill;
      raster.cullMode = vk::CullModeFlagBits::eNone;
      raster.frontFace = vk::FrontFace::eCounterClockwise;
      raster.lineWidth = 1.0F;
      auto multisample = vk::PipelineMultisampleStateCreateInfo{};
      multisample.rasterizationSamples = vk::SampleCountFlagBits::e1;
      auto depth = vk::PipelineDepthStencilStateCreateInfo{};
      depth.depthTestEnable = VK_TRUE;
      depth.depthWriteEnable = VK_TRUE;
      depth.depthCompareOp = vk::CompareOp::eLess;
      auto blend_attachment = vk::PipelineColorBlendAttachmentState{};
      blend_attachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                                        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
      auto blend_attachments = std::vector<vk::PipelineColorBlendAttachmentState>(color_formats.size(),
                                                                                  blend_attachment);
      auto blend = vk::PipelineColorBlendStateCreateInfo{};
      blend.attachmentCount = static_cast<std::uint32_t>(blend_attachments.size());
      blend.pAttachments = blend_attachments.data();
      constexpr std::array dynamic_states{vk::DynamicState::eViewport, vk::DynamicState::eScissor};
      auto dynamic = vk::PipelineDynamicStateCreateInfo{};
      dynamic.dynamicStateCount = static_cast<std::uint32_t>(dynamic_states.size());
      dynamic.pDynamicStates = dynamic_states.data();
      auto rendering = vk::PipelineRenderingCreateInfo{};
      rendering.colorAttachmentCount = static_cast<std::uint32_t>(color_formats.size());
      rendering.pColorAttachmentFormats = color_formats.data();
      rendering.depthAttachmentFormat = depth_format;
      auto info = vk::GraphicsPipelineCreateInfo{};
      info.pNext = &rendering;
      if (fragment_shader) {
         stages[1].stage = vk::ShaderStageFlagBits::eFragment;
         stages[1].module = fragment_shader;
         stages[1].pName = fragment_name.c_str();
      }
      info.stageCount = fragment_shader ? 2U : 1U;
      info.pStages = stages.data();
      info.pVertexInputState = &vertex_input;
      info.pInputAssemblyState = &assembly;
      info.pViewportState = &viewport_state;
      info.pRasterizationState = &raster;
      info.pMultisampleState = &multisample;
      info.pDepthStencilState = depth_enabled ? &depth : nullptr;
      info.pColorBlendState = color_formats.empty() ? nullptr : &blend;
      info.pDynamicState = &dynamic;
      info.layout = *layout;
      result = device.createGraphicsPipelines(nullptr, 1, &info, nullptr, pipeline);
      if (fragment_shader) { device.destroyShaderModule(fragment_shader); }
      device.destroyShaderModule(vertex_shader);
      if (result != vk::Result::eSuccess) { device.destroyPipelineLayout(*layout); }
      return result;
   }

   /// @brief Records a dynamic-rendering pass that clears then draws one hardcoded triangle.
   vk::Result recordSwapchainTriangle(vk::Device device, vk::CommandPool pool, vk::CommandBuffer command_buffer,
                                      vk::Image image, vk::ImageView view, vk::Extent2D extent,
                                      vk::ImageLayout old_layout, vk::Pipeline pipeline,
                                      const vk::ClearColorValue &clear_color) {
      auto result = detail::beginOneTime(device, pool, command_buffer);
      if (result != vk::Result::eSuccess) { return result; }

      auto to_attachment = detail::imageBarrier(image, vk::ImageAspectFlagBits::eColor, old_layout,
                                                vk::ImageLayout::eColorAttachmentOptimal,
                                                vk::PipelineStageFlagBits2::eNone, vk::AccessFlagBits2::eNone,
                                                vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                                                vk::AccessFlagBits2::eColorAttachmentWrite);
      detail::imageBarrier(command_buffer, to_attachment);

      auto attachment = detail::colorAttachment(view, clear_color);
      auto rendering = vk::RenderingInfo{};
      rendering.renderArea = vk::Rect2D{vk::Offset2D{0, 0}, extent};
      rendering.layerCount = 1;
      rendering.colorAttachmentCount = 1;
      rendering.pColorAttachments = &attachment;
      command_buffer.beginRendering(&rendering);
      detail::setViewportAndScissor(command_buffer, extent);
      command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
      command_buffer.draw(3, 1, 0, 0);
      command_buffer.endRendering();

      auto to_present = to_attachment;
      to_present.srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
      to_present.srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite;
      to_present.dstStageMask = vk::PipelineStageFlagBits2::eNone;
      to_present.dstAccessMask = vk::AccessFlagBits2::eNone;
      to_present.oldLayout = vk::ImageLayout::eColorAttachmentOptimal;
      to_present.newLayout = vk::ImageLayout::ePresentSrcKHR;
      detail::imageBarrier(command_buffer, to_present);
      return command_buffer.end();
   }

   /// @brief Records a dynamic-rendering pass that clears then draws uploaded indexed geometry.
   vk::Result recordSwapchainScene(vk::Device device, vk::CommandPool pool, vk::CommandBuffer command_buffer,
                                   vk::Image image, vk::ImageView view, vk::Extent2D extent,
                                   vk::ImageLayout old_layout, vk::Image depth_image, vk::ImageView depth_view,
                                   vk::ImageLayout depth_old_layout, vk::Image shadow_image,
                                   vk::ImageLayout shadow_old_layout, vk::PipelineLayout layout,
                                   vk::Pipeline pipeline, vk::Buffer vertex_buffer, vk::Buffer index_buffer,
                                   vk::DescriptorSet debug_set, vk::Buffer debug_buffer,
                                   vk::DeviceSize debug_buffer_size, std::uint32_t index_count,
                                   std::span<const float> scene_constants,
                                   const vk::ClearColorValue &clear_color) {
      if (scene_constants.size() < 52 || !debug_buffer) { return vk::Result::eErrorInitializationFailed; }

      auto result = detail::beginOneTime(device, pool, command_buffer);
      if (result != vk::Result::eSuccess) { return result; }

      auto to_attachment = detail::imageBarrier(image, vk::ImageAspectFlagBits::eColor, old_layout,
                                                vk::ImageLayout::eColorAttachmentOptimal,
                                                vk::PipelineStageFlagBits2::eNone, vk::AccessFlagBits2::eNone,
                                                vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                                                vk::AccessFlagBits2::eColorAttachmentWrite);
      detail::imageBarrier(command_buffer, to_attachment);

      auto depth_src_stage = vk::PipelineStageFlags2{};
      auto depth_src_access = vk::AccessFlags2{};
      if (depth_old_layout == vk::ImageLayout::eShaderReadOnlyOptimal) {
         depth_src_stage = vk::PipelineStageFlagBits2::eVertexShader |
                           vk::PipelineStageFlagBits2::eFragmentShader;
         depth_src_access = vk::AccessFlagBits2::eShaderSampledRead;
      } else if (depth_old_layout == vk::ImageLayout::eTransferSrcOptimal) {
         depth_src_stage = vk::PipelineStageFlagBits2::eCopy;
         depth_src_access = vk::AccessFlagBits2::eTransferRead;
      }
      auto to_depth = detail::imageBarrier(depth_image, vk::ImageAspectFlagBits::eDepth, depth_old_layout,
                                           vk::ImageLayout::eDepthStencilAttachmentOptimal,
                                           depth_src_stage, depth_src_access,
                                           vk::PipelineStageFlagBits2::eEarlyFragmentTests |
                                           vk::PipelineStageFlagBits2::eLateFragmentTests,
                                           vk::AccessFlagBits2::eDepthStencilAttachmentRead |
                                           vk::AccessFlagBits2::eDepthStencilAttachmentWrite);
      detail::imageBarrier(command_buffer, to_depth);

      auto shadow_to_shader = detail::imageBarrier(
         shadow_image, vk::ImageAspectFlagBits::eDepth, shadow_old_layout, vk::ImageLayout::eShaderReadOnlyOptimal,
         vk::PipelineStageFlagBits2::eCopy, vk::AccessFlagBits2::eTransferRead,
         vk::PipelineStageFlagBits2::eVertexShader | vk::PipelineStageFlagBits2::eFragmentShader,
         vk::AccessFlagBits2::eShaderSampledRead);
      detail::imageBarrier(command_buffer, shadow_to_shader);

      auto to_shader = detail::bufferBarrier(debug_buffer, debug_buffer_size, vk::PipelineStageFlagBits2::eHost,
                                             vk::AccessFlagBits2::eHostWrite,
                                             vk::PipelineStageFlagBits2::eVertexShader,
                                             vk::AccessFlagBits2::eShaderStorageRead |
                                             vk::AccessFlagBits2::eShaderStorageWrite);
      detail::bufferBarrier(command_buffer, to_shader);

      auto attachment = detail::colorAttachment(view, clear_color);
      auto depth_attachment = detail::depthAttachment(depth_view);
      auto rendering = vk::RenderingInfo{};
      rendering.renderArea = vk::Rect2D{vk::Offset2D{0, 0}, extent};
      rendering.layerCount = 1;
      rendering.colorAttachmentCount = 1;
      rendering.pColorAttachments = &attachment;
      rendering.pDepthAttachment = &depth_attachment;
      command_buffer.beginRendering(&rendering);

      const auto vertex_offset = vk::DeviceSize{};
      detail::setViewportAndScissor(command_buffer, extent);
      command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
      command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, layout, 0, 1, &debug_set, 0, nullptr);
      command_buffer.pushConstants(layout, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0,
                                   52U * static_cast<std::uint32_t>(sizeof(float)),
                                   scene_constants.data());
      command_buffer.bindVertexBuffers(0, 1, &vertex_buffer, &vertex_offset);
      command_buffer.bindIndexBuffer(index_buffer, 0, vk::IndexType::eUint32);
      command_buffer.drawIndexed(index_count, 1, 0, 0, 0);
      command_buffer.endRendering();

      auto to_host = detail::bufferBarrier(debug_buffer, debug_buffer_size, vk::PipelineStageFlagBits2::eVertexShader,
                                           vk::AccessFlagBits2::eShaderStorageWrite,
                                           vk::PipelineStageFlagBits2::eHost,
                                           vk::AccessFlagBits2::eHostRead);
      detail::bufferBarrier(command_buffer, to_host);

      auto to_present = to_attachment;
      to_present.srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
      to_present.srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite;
      to_present.dstStageMask = vk::PipelineStageFlagBits2::eNone;
      to_present.dstAccessMask = vk::AccessFlagBits2::eNone;
      to_present.oldLayout = vk::ImageLayout::eColorAttachmentOptimal;
      to_present.newLayout = vk::ImageLayout::ePresentSrcKHR;
      detail::imageBarrier(command_buffer, to_present);
      return command_buffer.end();
   }

   /// @brief Records a depth-only scene pass and copies the shadow depth image to a host buffer.
   vk::Result recordSceneShadowDepth(vk::Device device, vk::CommandPool pool, vk::CommandBuffer command_buffer,
                                     vk::Image depth_image, vk::ImageView depth_view, vk::Extent2D extent,
                                     vk::ImageLayout old_layout, vk::PipelineLayout layout,
                                     vk::Pipeline pipeline, vk::Buffer vertex_buffer, vk::Buffer index_buffer,
                                     std::uint32_t index_count, std::span<const float> scene_constants,
                                     vk::Buffer readback_buffer) {
      if (scene_constants.size() < 52 || !readback_buffer) { return vk::Result::eErrorInitializationFailed; }

      auto result = detail::beginOneTime(device, pool, command_buffer);
      if (result != vk::Result::eSuccess) { return result; }

      auto depth_src_stage = vk::PipelineStageFlags2{};
      auto depth_src_access = vk::AccessFlags2{};
      if (old_layout == vk::ImageLayout::eShaderReadOnlyOptimal) {
         depth_src_stage = vk::PipelineStageFlagBits2::eVertexShader |
                           vk::PipelineStageFlagBits2::eFragmentShader;
         depth_src_access = vk::AccessFlagBits2::eShaderSampledRead;
      } else if (old_layout == vk::ImageLayout::eTransferSrcOptimal) {
         depth_src_stage = vk::PipelineStageFlagBits2::eCopy;
         depth_src_access = vk::AccessFlagBits2::eTransferRead;
      }
      auto to_depth = detail::imageBarrier(depth_image, vk::ImageAspectFlagBits::eDepth, old_layout,
                                           vk::ImageLayout::eDepthStencilAttachmentOptimal,
                                           depth_src_stage, depth_src_access,
                                           vk::PipelineStageFlagBits2::eEarlyFragmentTests |
                                           vk::PipelineStageFlagBits2::eLateFragmentTests,
                                           vk::AccessFlagBits2::eDepthStencilAttachmentRead |
                                           vk::AccessFlagBits2::eDepthStencilAttachmentWrite);
      detail::imageBarrier(command_buffer, to_depth);

      auto depth_attachment = detail::depthAttachment(depth_view);
      auto rendering = vk::RenderingInfo{};
      rendering.renderArea = vk::Rect2D{vk::Offset2D{0, 0}, extent};
      rendering.layerCount = 1;
      rendering.pDepthAttachment = &depth_attachment;
      command_buffer.beginRendering(&rendering);

      const auto vertex_offset = vk::DeviceSize{};
      detail::setViewportAndScissor(command_buffer, extent);
      command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
      command_buffer.pushConstants(layout, vk::ShaderStageFlagBits::eVertex, 0,
                                   52U * static_cast<std::uint32_t>(sizeof(float)),
                                   scene_constants.data());
      command_buffer.bindVertexBuffers(0, 1, &vertex_buffer, &vertex_offset);
      command_buffer.bindIndexBuffer(index_buffer, 0, vk::IndexType::eUint32);
      command_buffer.drawIndexed(index_count, 1, 0, 0, 0);
      command_buffer.endRendering();

      auto to_transfer = to_depth;
      to_transfer.srcStageMask = vk::PipelineStageFlagBits2::eLateFragmentTests;
      to_transfer.srcAccessMask = vk::AccessFlagBits2::eDepthStencilAttachmentWrite;
      to_transfer.dstStageMask = vk::PipelineStageFlagBits2::eCopy;
      to_transfer.dstAccessMask = vk::AccessFlagBits2::eTransferRead;
      to_transfer.oldLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
      to_transfer.newLayout = vk::ImageLayout::eTransferSrcOptimal;
      detail::imageBarrier(command_buffer, to_transfer);

      auto subresource = vk::ImageSubresourceLayers{};
      subresource.aspectMask = vk::ImageAspectFlagBits::eDepth;
      subresource.layerCount = 1;
      auto region = vk::BufferImageCopy{};
      region.imageSubresource = subresource;
      region.imageExtent = vk::Extent3D{extent.width, extent.height, 1};
      command_buffer.copyImageToBuffer(depth_image, vk::ImageLayout::eTransferSrcOptimal,
                                       readback_buffer, 1, &region);

      auto to_host = detail::bufferBarrier(readback_buffer, VK_WHOLE_SIZE, vk::PipelineStageFlagBits2::eCopy,
                                           vk::AccessFlagBits2::eTransferWrite,
                                           vk::PipelineStageFlagBits2::eHost,
                                           vk::AccessFlagBits2::eHostRead);
      detail::bufferBarrier(command_buffer, to_host);
      return command_buffer.end();
   }

   /// @brief Submits one command buffer, signals a timeline value, and waits for that value on the host.
   vk::Result submitAndWait(vk::Device device, vk::Queue queue, vk::CommandBuffer command_buffer,
                            vk::Semaphore timeline, std::uint64_t value) {
      auto command = vk::CommandBufferSubmitInfo{};
      command.commandBuffer = command_buffer;
      auto signal = vk::SemaphoreSubmitInfo{};
      signal.semaphore = timeline;
      signal.value = value;
      signal.stageMask = vk::PipelineStageFlagBits2::eAllCommands;
      auto submit = vk::SubmitInfo2{};
      submit.commandBufferInfoCount = 1;
      submit.pCommandBufferInfos = &command;
      submit.signalSemaphoreInfoCount = 1;
      submit.pSignalSemaphoreInfos = &signal;
      const auto result = queue.submit2(1, &submit, nullptr);
      if (result != vk::Result::eSuccess) { return result; }

      auto wait = vk::SemaphoreWaitInfo{};
      wait.semaphoreCount = 1;
      wait.pSemaphores = &timeline;
      wait.pValues = &value;
      return device.waitSemaphores(&wait, std::numeric_limits<std::uint64_t>::max());
   }

} // namespace vve::v4::vh::low
