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
   createFrameExecutor(vk::Device device, std::uint32_t queue_family, vk::CommandPool *pool,
                       vk::CommandBuffer *command_buffer, vk::Semaphore *timeline, vk::Fence *acquire_fence);
   [[nodiscard]] vk::Result
   recordSwapchainClear(vk::Device device, vk::CommandPool pool, vk::CommandBuffer command_buffer, vk::Image image,
                        vk::ImageView view, vk::Extent2D extent, vk::ImageLayout old_layout,
                        const vk::ClearColorValue &clear_color);
   [[nodiscard]] vk::Result
   createTrianglePipeline(vk::Device device, vk::Format color_format, std::span<const std::uint32_t> vertex_spirv,
                          std::string_view vertex_entry, std::span<const std::uint32_t> fragment_spirv,
                          std::string_view fragment_entry, vk::PipelineLayout *layout, vk::Pipeline *pipeline);
   [[nodiscard]] vk::Result
   recordSwapchainTriangle(vk::Device device, vk::CommandPool pool, vk::CommandBuffer command_buffer,
                           vk::Image image, vk::ImageView view, vk::Extent2D extent, vk::ImageLayout old_layout,
                           vk::Pipeline pipeline, const vk::ClearColorValue &clear_color);
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
      auto features2 = vk::PhysicalDeviceFeatures2{};
      const auto api_version = device.getProperties().apiVersion;
      features2.pNext = &features12;
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
                            .maintenance6 = features14.maintenance6 == VK_TRUE};
   }

   [[nodiscard]] bool hasRequiredModernFeatures(const ModernFeatures &features) {
      const auto indexed_sets = features.descriptor_indexing && features.sampled_image_update_after_bind &&
                                features.storage_buffer_update_after_bind && features.partially_bound &&
                                features.variable_descriptor_count && features.runtime_descriptor_array;
      return features.timeline_semaphore && features.synchronization2 && features.dynamic_rendering &&
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
                                       features.maintenance6};
      return static_cast<std::uint32_t>(std::ranges::count(optional, true));
   }

   [[nodiscard]] std::uint32_t descriptorModelRank(const ModernFeatures &features) {
      if (features.descriptor_buffer) { return 2; }
      if (hasRequiredModernFeatures(features)) { return 1; }
      return 0;
   }

   void enableModernFeatures(const ModernFeatures &support, std::uint32_t api_version,
                             vk::PhysicalDeviceFeatures2 &features2,
                             vk::PhysicalDeviceVulkan12Features &features12,
                             vk::PhysicalDeviceVulkan13Features &features13,
                             vk::PhysicalDeviceVulkan14Features &features14,
                             vk::PhysicalDeviceDescriptorBufferFeaturesEXT &descriptor_buffer) {
      features2.pNext = &features12;
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
#ifdef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
            if (hasExtension(available, VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME)) {
               detail::appendUnique(enabled, VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
            }
#endif
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
      auto extension_names = std::vector<const char *>{};
      extension_names.reserve(enabled.size());
      for (const auto &extension : enabled) { extension_names.push_back(extension.c_str()); }

      auto features14 = vk::PhysicalDeviceVulkan14Features{};
      auto features13 = vk::PhysicalDeviceVulkan13Features{};
      auto features12 = vk::PhysicalDeviceVulkan12Features{};
      auto descriptor_buffer = vk::PhysicalDeviceDescriptorBufferFeaturesEXT{};
      auto features2 = vk::PhysicalDeviceFeatures2{};
      detail::enableModernFeatures(support, api_version, features2, features12, features13, features14,
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

   /// @brief Creates a device-local depth image, allocates memory, binds it, and creates its view.
   vk::Result createDepthTarget(vk::PhysicalDevice physical_device, vk::Device device, vk::Extent2D extent,
                                vk::Format *format, vk::Image *image, vk::DeviceMemory *memory,
                                vk::ImageView *view) {
      constexpr auto formats = std::array{vk::Format::eD32Sfloat, vk::Format::eD24UnormS8Uint,
                                          vk::Format::eD32SfloatS8Uint};
      const auto it = std::ranges::find_if(formats, [&](vk::Format candidate) {
         const auto props = physical_device.getFormatProperties(candidate);
         return (props.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment) ==
                vk::FormatFeatureFlagBits::eDepthStencilAttachment;
      });
      if (it == formats.end()) { return vk::Result::eErrorFormatNotSupported; }

      *format = *it;
      auto image_info = vk::ImageCreateInfo{};
      image_info.imageType = vk::ImageType::e2D;
      image_info.format = *format;
      image_info.extent = vk::Extent3D{extent.width, extent.height, 1};
      image_info.mipLevels = 1;
      image_info.arrayLayers = 1;
      image_info.samples = vk::SampleCountFlagBits::e1;
      image_info.tiling = vk::ImageTiling::eOptimal;
      image_info.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;
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
      range.aspectMask = vk::ImageAspectFlagBits::eDepth;
      range.levelCount = 1;
      range.layerCount = 1;
      auto view_info = vk::ImageViewCreateInfo{};
      view_info.image = *image;
      view_info.viewType = vk::ImageViewType::e2D;
      view_info.format = *format;
      view_info.subresourceRange = range;
      return device.createImageView(&view_info, nullptr, view);
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

   /// @brief Records a dynamic-rendering pass that clears one acquired swapchain image.
   vk::Result recordSwapchainClear(vk::Device device, vk::CommandPool pool, vk::CommandBuffer command_buffer,
                                   vk::Image image, vk::ImageView view, vk::Extent2D extent,
                                   vk::ImageLayout old_layout, const vk::ClearColorValue &clear_color) {
      auto result = device.resetCommandPool(pool);
      if (result != vk::Result::eSuccess) { return result; }

      auto begin = vk::CommandBufferBeginInfo{};
      begin.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
      result = command_buffer.begin(&begin);
      if (result != vk::Result::eSuccess) { return result; }

      auto range = vk::ImageSubresourceRange{};
      range.aspectMask = vk::ImageAspectFlagBits::eColor;
      range.levelCount = 1;
      range.layerCount = 1;

      auto to_attachment = vk::ImageMemoryBarrier2{};
      to_attachment.srcStageMask = vk::PipelineStageFlagBits2::eNone;
      to_attachment.srcAccessMask = vk::AccessFlagBits2::eNone;
      to_attachment.dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
      to_attachment.dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite;
      to_attachment.oldLayout = old_layout;
      to_attachment.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
      to_attachment.image = image;
      to_attachment.subresourceRange = range;
      auto dependency = vk::DependencyInfo{};
      dependency.imageMemoryBarrierCount = 1;
      dependency.pImageMemoryBarriers = &to_attachment;
      command_buffer.pipelineBarrier2(&dependency);

      auto clear = vk::ClearValue{};
      clear.color = clear_color;
      auto attachment = vk::RenderingAttachmentInfo{};
      attachment.imageView = view;
      attachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
      attachment.loadOp = vk::AttachmentLoadOp::eClear;
      attachment.storeOp = vk::AttachmentStoreOp::eStore;
      attachment.clearValue = clear;
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
      dependency.pImageMemoryBarriers = &to_present;
      command_buffer.pipelineBarrier2(&dependency);
      return command_buffer.end();
   }

   /// @brief Creates a no-input graphics pipeline for the hardcoded triangle shader.
   vk::Result createTrianglePipeline(vk::Device device, vk::Format color_format,
                                     std::span<const std::uint32_t> vertex_spirv,
                                     std::string_view vertex_entry,
                                     std::span<const std::uint32_t> fragment_spirv,
                                     std::string_view fragment_entry,
                                     vk::PipelineLayout *layout, vk::Pipeline *pipeline) {
      auto shader_info = vk::ShaderModuleCreateInfo{};
      shader_info.codeSize = vertex_spirv.size_bytes();
      shader_info.pCode = vertex_spirv.data();
      vk::ShaderModule vertex_shader{};
      auto result = device.createShaderModule(&shader_info, nullptr, &vertex_shader);
      if (result != vk::Result::eSuccess) { return result; }

      shader_info.codeSize = fragment_spirv.size_bytes();
      shader_info.pCode = fragment_spirv.data();
      vk::ShaderModule fragment_shader{};
      result = device.createShaderModule(&shader_info, nullptr, &fragment_shader);
      if (result != vk::Result::eSuccess) {
         device.destroyShaderModule(vertex_shader);
         return result;
      }

      auto layout_info = vk::PipelineLayoutCreateInfo{};
      result = device.createPipelineLayout(&layout_info, nullptr, layout);
      if (result != vk::Result::eSuccess) {
         device.destroyShaderModule(fragment_shader);
         device.destroyShaderModule(vertex_shader);
         return result;
      }

      const auto vertex_name = std::string{vertex_entry};
      const auto fragment_name = std::string{fragment_entry};
      auto stages = std::array{vk::PipelineShaderStageCreateInfo{}, vk::PipelineShaderStageCreateInfo{}};
      stages[0].stage = vk::ShaderStageFlagBits::eVertex;
      stages[0].module = vertex_shader;
      stages[0].pName = vertex_name.c_str();
      stages[1].stage = vk::ShaderStageFlagBits::eFragment;
      stages[1].module = fragment_shader;
      stages[1].pName = fragment_name.c_str();
      auto vertex_input = vk::PipelineVertexInputStateCreateInfo{};
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
      auto blend_attachment = vk::PipelineColorBlendAttachmentState{};
      blend_attachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                                        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
      auto blend = vk::PipelineColorBlendStateCreateInfo{};
      blend.attachmentCount = 1;
      blend.pAttachments = &blend_attachment;
      constexpr std::array dynamic_states{vk::DynamicState::eViewport, vk::DynamicState::eScissor};
      auto dynamic = vk::PipelineDynamicStateCreateInfo{};
      dynamic.dynamicStateCount = static_cast<std::uint32_t>(dynamic_states.size());
      dynamic.pDynamicStates = dynamic_states.data();
      auto rendering = vk::PipelineRenderingCreateInfo{};
      rendering.colorAttachmentCount = 1;
      rendering.pColorAttachmentFormats = &color_format;
      auto info = vk::GraphicsPipelineCreateInfo{};
      info.pNext = &rendering;
      info.stageCount = static_cast<std::uint32_t>(stages.size());
      info.pStages = stages.data();
      info.pVertexInputState = &vertex_input;
      info.pInputAssemblyState = &assembly;
      info.pViewportState = &viewport_state;
      info.pRasterizationState = &raster;
      info.pMultisampleState = &multisample;
      info.pColorBlendState = &blend;
      info.pDynamicState = &dynamic;
      info.layout = *layout;
      result = device.createGraphicsPipelines(nullptr, 1, &info, nullptr, pipeline);
      device.destroyShaderModule(fragment_shader);
      device.destroyShaderModule(vertex_shader);
      if (result != vk::Result::eSuccess) { device.destroyPipelineLayout(*layout); }
      return result;
   }

   /// @brief Records a dynamic-rendering pass that clears then draws one hardcoded triangle.
   vk::Result recordSwapchainTriangle(vk::Device device, vk::CommandPool pool, vk::CommandBuffer command_buffer,
                                      vk::Image image, vk::ImageView view, vk::Extent2D extent,
                                      vk::ImageLayout old_layout, vk::Pipeline pipeline,
                                      const vk::ClearColorValue &clear_color) {
      auto result = device.resetCommandPool(pool);
      if (result != vk::Result::eSuccess) { return result; }

      auto begin = vk::CommandBufferBeginInfo{};
      begin.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
      result = command_buffer.begin(&begin);
      if (result != vk::Result::eSuccess) { return result; }

      auto range = vk::ImageSubresourceRange{};
      range.aspectMask = vk::ImageAspectFlagBits::eColor;
      range.levelCount = 1;
      range.layerCount = 1;
      auto to_attachment = vk::ImageMemoryBarrier2{};
      to_attachment.dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
      to_attachment.dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite;
      to_attachment.oldLayout = old_layout;
      to_attachment.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
      to_attachment.image = image;
      to_attachment.subresourceRange = range;
      auto dependency = vk::DependencyInfo{};
      dependency.imageMemoryBarrierCount = 1;
      dependency.pImageMemoryBarriers = &to_attachment;
      command_buffer.pipelineBarrier2(&dependency);

      auto clear = vk::ClearValue{};
      clear.color = clear_color;
      auto attachment = vk::RenderingAttachmentInfo{};
      attachment.imageView = view;
      attachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
      attachment.loadOp = vk::AttachmentLoadOp::eClear;
      attachment.storeOp = vk::AttachmentStoreOp::eStore;
      attachment.clearValue = clear;
      auto rendering = vk::RenderingInfo{};
      rendering.renderArea = vk::Rect2D{vk::Offset2D{0, 0}, extent};
      rendering.layerCount = 1;
      rendering.colorAttachmentCount = 1;
      rendering.pColorAttachments = &attachment;
      command_buffer.beginRendering(&rendering);
      auto viewport = vk::Viewport{0.0F, 0.0F, static_cast<float>(extent.width), static_cast<float>(extent.height),
                                   0.0F, 1.0F};
      auto scissor = vk::Rect2D{vk::Offset2D{0, 0}, extent};
      command_buffer.setViewport(0, 1, &viewport);
      command_buffer.setScissor(0, 1, &scissor);
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
      dependency.pImageMemoryBarriers = &to_present;
      command_buffer.pipelineBarrier2(&dependency);
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
