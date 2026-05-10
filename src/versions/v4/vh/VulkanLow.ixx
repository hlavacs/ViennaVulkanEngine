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

} // namespace vve::v4::vh::low
