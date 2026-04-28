module;

#include <vulkan/vulkan.h>

#include <cstring>

module vh.low;
import std;

namespace {

   [[nodiscard]] VkApplicationInfo makeApplicationInfo(const vh::low::InstanceProfile &profile,
                                                       std::uint32_t api_version) noexcept {
      return VkApplicationInfo{.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                               .pNext = nullptr,
                               .pApplicationName = profile.application_name,
                               .applicationVersion = profile.application_version,
                               .pEngineName = profile.engine_name,
                               .engineVersion = profile.engine_version,
                               .apiVersion = api_version};
   }

   [[nodiscard]] VkInstanceCreateInfo makeInstanceCreateInfo(const vh::low::InstanceProfile &profile,
                                                             const vh::low::InstancePlan &plan,
                                                             const VkApplicationInfo &application_info) noexcept {
      return VkInstanceCreateInfo{.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                                  .pNext = profile.next,
                                  .flags = plan.flags,
                                  .pApplicationInfo = &application_info,
                                  .enabledLayerCount = static_cast<std::uint32_t>(plan.layers.size()),
                                  .ppEnabledLayerNames = plan.layers.data(),
                                  .enabledExtensionCount = static_cast<std::uint32_t>(plan.extensions.size()),
                                  .ppEnabledExtensionNames = plan.extensions.data()};
   }

   [[nodiscard]] VkDeviceQueueCreateInfo makeDeviceQueueCreateInfo(std::uint32_t queue_family,
                                                                   const float *priority) noexcept {
      return VkDeviceQueueCreateInfo{.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                     .pNext = nullptr,
                                     .flags = 0,
                                     .queueFamilyIndex = queue_family,
                                     .queueCount = 1,
                                     .pQueuePriorities = priority};
   }

   [[nodiscard]] VkBufferCreateInfo makeBufferCreateInfo(VkDeviceSize size,
                                                         VkBufferUsageFlags usage) noexcept {
      return VkBufferCreateInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                .pNext = nullptr,
                                .flags = 0,
                                .size = size,
                                .usage = usage,
                                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                .queueFamilyIndexCount = 0,
                                .pQueueFamilyIndices = nullptr};
   }

   [[nodiscard]] VkImageSubresourceRange makeImageSubresourceRange(VkImageAspectFlags aspect_mask,
                                                                   std::uint32_t mip_levels,
                                                                   std::uint32_t array_layers) noexcept {
      return VkImageSubresourceRange{.aspectMask = aspect_mask,
                                     .baseMipLevel = 0,
                                     .levelCount = mip_levels,
                                     .baseArrayLayer = 0,
                                     .layerCount = array_layers};
   }

   [[nodiscard]] VkImageCreateInfo makeImageCreateInfo2D(const vh::low::Image2DAllocationRequest &request) noexcept {
      return VkImageCreateInfo{.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                               .pNext = nullptr,
                               .flags = 0,
                               .imageType = VK_IMAGE_TYPE_2D,
                               .format = request.format,
                               .extent = {.width = request.width, .height = request.height, .depth = 1},
                               .mipLevels = request.mip_levels,
                               .arrayLayers = request.array_layers,
                               .samples = VK_SAMPLE_COUNT_1_BIT,
                               .tiling = VK_IMAGE_TILING_OPTIMAL,
                               .usage = request.usage,
                               .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                               .queueFamilyIndexCount = 0,
                               .pQueueFamilyIndices = nullptr,
                               .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};
   }

   [[nodiscard]] VkImageViewCreateInfo makeImageViewCreateInfo2D(
       VkImage image, const vh::low::Image2DAllocationRequest &request) noexcept {
      return VkImageViewCreateInfo{.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                   .pNext = nullptr,
                                   .flags = 0,
                                   .image = image,
                                   .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                   .format = request.format,
                                   .components = {.r = VK_COMPONENT_SWIZZLE_IDENTITY,
                                                  .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                                                  .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                                                  .a = VK_COMPONENT_SWIZZLE_IDENTITY},
                                   .subresourceRange = makeImageSubresourceRange(request.aspect_mask,
                                                                                 request.mip_levels,
                                                                                 request.array_layers)};
   }

   [[nodiscard]] VkSamplerCreateInfo makeLinearRepeatSamplerCreateInfo() noexcept {
      return VkSamplerCreateInfo{.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                 .pNext = nullptr,
                                 .flags = 0,
                                 .magFilter = VK_FILTER_LINEAR,
                                 .minFilter = VK_FILTER_LINEAR,
                                 .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
                                 .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                 .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                 .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                 .mipLodBias = 0.0F,
                                 .anisotropyEnable = VK_FALSE,
                                 .maxAnisotropy = 1.0F,
                                 .compareEnable = VK_FALSE,
                                 .compareOp = VK_COMPARE_OP_ALWAYS,
                                 .minLod = 0.0F,
                                 .maxLod = 0.0F,
                                 .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
                                 .unnormalizedCoordinates = VK_FALSE};
   }

   [[nodiscard]] std::uint32_t deviceTypeScore(VkPhysicalDeviceType type, bool prefer_discrete_gpu) noexcept {
      if (prefer_discrete_gpu && type == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
         return 1000U;
      }
      if (type == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
         return 700U;
      }
      if (!prefer_discrete_gpu && type == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
         return 650U;
      }
      if (type == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU) {
         return 450U;
      }
      if (type == VK_PHYSICAL_DEVICE_TYPE_CPU) {
         return 100U;
      }

      return 250U;
   }

   [[nodiscard]] bool hasRequiredNames(std::span<const char *const> required,
                                       std::span<const VkExtensionProperties> available) noexcept {
      return std::ranges::all_of(required, [available](const char *name) {
         return vh::low::hasExtension(available, name);
      });
   }

   void appendAvailableOptionalNames(std::vector<const char *> &enabled,
                                     std::span<const char *const> optional,
                                     std::span<const VkExtensionProperties> available) {
      for (const char *name : optional) {
         if (vh::low::hasExtension(available, name)) {
            vh::low::appendUniqueName(enabled, name);
         }
      }
   }

} // namespace

namespace vh::low {

   std::string versionString(std::uint32_t version) {
      return std::to_string(VK_API_VERSION_MAJOR(version)) + "." +
             std::to_string(VK_API_VERSION_MINOR(version)) + "." +
             std::to_string(VK_API_VERSION_PATCH(version));
   }

   std::uint32_t chooseApiVersion(std::uint32_t available_version, ApiVersionPolicy policy) noexcept {
      const std::uint32_t highest_allowed = std::min(available_version, policy.maximum);
      if (highest_allowed < policy.minimum) {
         return 0;
      }
      return std::min(policy.preferred, highest_allowed);
   }

   VkResult querySupportedApiVersion(ApiVersionPolicy policy, std::uint32_t *selected_version) noexcept {
      if (selected_version == nullptr) {
         return VK_ERROR_INITIALIZATION_FAILED;
      }

      std::uint32_t available_version = VK_API_VERSION_1_0;
      const auto enumerate_instance_version = reinterpret_cast<PFN_vkEnumerateInstanceVersion>(
          vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceVersion"));
      if (enumerate_instance_version != nullptr) {
         if (const VkResult result = enumerate_instance_version(&available_version); result != VK_SUCCESS) {
            return result;
         }
      }

      *selected_version = chooseApiVersion(available_version, policy);
      return *selected_version == 0 ? VK_ERROR_INCOMPATIBLE_DRIVER : VK_SUCCESS;
   }

   bool hasName(std::span<const char *const> names, std::string_view name) noexcept {
      if (name.empty()) {
         return false;
      }

      return std::ranges::any_of(names, [name](const char *candidate) {
         return candidate != nullptr && std::string_view{candidate} == name;
      });
   }

   void appendUniqueName(std::vector<const char *> &names, const char *name) {
      if (name == nullptr || *name == '\0' ||
          hasName(std::span<const char *const>{names.data(), names.size()}, name)) {
         return;
      }

      names.push_back(name);
   }

   bool hasExtension(std::span<const VkExtensionProperties> extensions, std::string_view name) noexcept {
      if (name.empty()) {
         return false;
      }

      return std::ranges::any_of(extensions, [name](const VkExtensionProperties &extension) {
         return std::string_view{extension.extensionName} == name;
      });
   }

   bool hasLayer(std::span<const VkLayerProperties> layers, std::string_view name) noexcept {
      if (name.empty()) {
         return false;
      }

      return std::ranges::any_of(layers, [name](const VkLayerProperties &layer) {
         return std::string_view{layer.layerName} == name;
      });
   }

   bool isSuccessfulEnumerationResult(VkResult result) noexcept {
      return result == VK_SUCCESS || result == VK_INCOMPLETE;
   }

   VkResult enumerateInstanceExtensions(std::vector<VkExtensionProperties> &extensions) noexcept {
      try {
         extensions.clear();
         std::uint32_t extension_count = 0;
         VkResult result = vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);
         if (result != VK_SUCCESS) {
            return result;
         }

         extensions.resize(extension_count);
         if (extension_count == 0) {
            return VK_SUCCESS;
         }

         result = vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, extensions.data());
         if (!isSuccessfulEnumerationResult(result)) {
            extensions.clear();
            return result;
         }

         extensions.resize(extension_count);
         return VK_SUCCESS;
      } catch (const std::bad_alloc &) {
         extensions.clear();
         return VK_ERROR_OUT_OF_HOST_MEMORY;
      }
   }

   VkResult enumerateInstanceLayers(std::vector<VkLayerProperties> &layers) noexcept {
      try {
         layers.clear();
         std::uint32_t layer_count = 0;
         VkResult result = vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
         if (result != VK_SUCCESS) {
            return result;
         }

         layers.resize(layer_count);
         if (layer_count == 0) {
            return VK_SUCCESS;
         }

         result = vkEnumerateInstanceLayerProperties(&layer_count, layers.data());
         if (!isSuccessfulEnumerationResult(result)) {
            layers.clear();
            return result;
         }

         layers.resize(layer_count);
         return VK_SUCCESS;
      } catch (const std::bad_alloc &) {
         layers.clear();
         return VK_ERROR_OUT_OF_HOST_MEMORY;
      }
   }

   VkResult planInstance(const InstanceProfile &profile, InstancePlan &plan) noexcept {
      try {
         plan = {};
         if (const VkResult version_result = querySupportedApiVersion(profile.api_version, &plan.api_version);
             version_result != VK_SUCCESS) {
            return version_result;
         }

         std::vector<VkExtensionProperties> available_extensions{};
         if (const VkResult result = enumerateInstanceExtensions(available_extensions); result != VK_SUCCESS) {
            return result;
         }
         const std::span<const VkExtensionProperties> extension_span{available_extensions};

         for (const char *name : profile.required_extensions) {
            if (!hasExtension(extension_span, name)) {
               return VK_ERROR_EXTENSION_NOT_PRESENT;
            }
            appendUniqueName(plan.extensions, name);
         }
         for (const char *name : profile.optional_extensions) {
            if (hasExtension(extension_span, name)) {
               appendUniqueName(plan.extensions, name);
            }
         }

#ifdef VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
         if (profile.enable_portability_enumeration &&
             hasExtension(extension_span, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)) {
            appendUniqueName(plan.extensions, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
            plan.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
         }
#endif

         if (!profile.required_layers.empty()) {
            std::vector<VkLayerProperties> available_layers{};
            if (const VkResult result = enumerateInstanceLayers(available_layers); result != VK_SUCCESS) {
               return result;
            }

            const std::span<const VkLayerProperties> layer_span{available_layers};
            for (const char *name : profile.required_layers) {
               if (!hasLayer(layer_span, name)) {
                  return VK_ERROR_LAYER_NOT_PRESENT;
               }
               appendUniqueName(plan.layers, name);
            }
         }

         return VK_SUCCESS;
      } catch (const std::bad_alloc &) {
         plan = {};
         return VK_ERROR_OUT_OF_HOST_MEMORY;
      }
   }

   VkResult createInstance(const InstanceProfile &profile, InstancePlan *plan, VkInstance *instance) noexcept {
      if (instance == nullptr) {
         return VK_ERROR_INITIALIZATION_FAILED;
      }

      InstancePlan local_plan{};
      if (const VkResult result = planInstance(profile, local_plan); result != VK_SUCCESS) {
         return result;
      }

      const auto application_info = makeApplicationInfo(profile, local_plan.api_version);
      const auto create_info = makeInstanceCreateInfo(profile, local_plan, application_info);
      const VkResult result = vkCreateInstance(&create_info, nullptr, instance);
      if (result != VK_SUCCESS) {
         return result;
      }

      if (plan != nullptr) {
         *plan = std::move(local_plan);
      }
      return VK_SUCCESS;
   }

   VkResult enumeratePhysicalDevices(VkInstance instance, std::vector<VkPhysicalDevice> &devices) noexcept {
      try {
         devices.clear();
         if (instance == VK_NULL_HANDLE) {
            return VK_ERROR_INITIALIZATION_FAILED;
         }

         std::uint32_t device_count = 0;
         VkResult result = vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
         if (result != VK_SUCCESS) {
            return result;
         }

         devices.resize(device_count);
         if (device_count == 0) {
            return VK_SUCCESS;
         }

         result = vkEnumeratePhysicalDevices(instance, &device_count, devices.data());
         if (!isSuccessfulEnumerationResult(result)) {
            devices.clear();
            return result;
         }

         devices.resize(device_count);
         return VK_SUCCESS;
      } catch (const std::bad_alloc &) {
         devices.clear();
         return VK_ERROR_OUT_OF_HOST_MEMORY;
      }
   }

   VkResult enumerateQueueFamilies(VkPhysicalDevice device,
                                   std::vector<VkQueueFamilyProperties> &queue_families) noexcept {
      try {
         queue_families.clear();
         if (device == VK_NULL_HANDLE) {
            return VK_ERROR_INITIALIZATION_FAILED;
         }

         std::uint32_t queue_family_count = 0;
         vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);
         queue_families.resize(queue_family_count);
         if (queue_family_count == 0) {
            return VK_SUCCESS;
         }

         vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());
         queue_families.resize(queue_family_count);
         return VK_SUCCESS;
      } catch (const std::bad_alloc &) {
         queue_families.clear();
         return VK_ERROR_OUT_OF_HOST_MEMORY;
      }
   }

   VkResult enumerateDeviceExtensions(VkPhysicalDevice device,
                                      std::vector<VkExtensionProperties> &extensions) noexcept {
      try {
         extensions.clear();
         if (device == VK_NULL_HANDLE) {
            return VK_ERROR_INITIALIZATION_FAILED;
         }

         std::uint32_t extension_count = 0;
         VkResult result = vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr);
         if (result != VK_SUCCESS) {
            return result;
         }

         extensions.resize(extension_count);
         if (extension_count == 0) {
            return VK_SUCCESS;
         }

         result = vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, extensions.data());
         if (!isSuccessfulEnumerationResult(result)) {
            extensions.clear();
            return result;
         }

         extensions.resize(extension_count);
         return VK_SUCCESS;
      } catch (const std::bad_alloc &) {
         extensions.clear();
         return VK_ERROR_OUT_OF_HOST_MEMORY;
      }
   }

   VkResult selectPhysicalDevice(const PhysicalDeviceSelectionRequest &request,
                                 PhysicalDeviceSelection &selection) noexcept {
      try {
         selection = {};
         std::vector<VkPhysicalDevice> devices{};
         if (const VkResult result = enumeratePhysicalDevices(request.instance, devices); result != VK_SUCCESS) {
            return result;
         }
         if (devices.empty()) {
            return VK_ERROR_INITIALIZATION_FAILED;
         }

         bool found = false;
         PhysicalDeviceSelection best{};
         for (const auto device : devices) {
            std::vector<VkExtensionProperties> available_extensions{};
            if (const VkResult result = enumerateDeviceExtensions(device, available_extensions); result != VK_SUCCESS) {
               return result;
            }
            const std::span<const VkExtensionProperties> extension_span{available_extensions};
            if (!hasRequiredNames(request.required_extensions, extension_span)) {
               continue;
            }

            std::vector<VkQueueFamilyProperties> queue_families{};
            if (const VkResult result = enumerateQueueFamilies(device, queue_families); result != VK_SUCCESS) {
               return result;
            }

            std::optional<std::uint32_t> selected_queue{};
            for (std::uint32_t index = 0; index < queue_families.size(); ++index) {
               const auto &queue_family = queue_families[index];
               if ((queue_family.queueFlags & request.required_queue_flags) != request.required_queue_flags ||
                   queue_family.queueCount == 0) {
                  continue;
               }
               if (request.presentation_support != nullptr &&
                   request.presentation_support(request.instance, device, index, request.presentation_context) !=
                       VK_TRUE) {
                  continue;
               }

               selected_queue = index;
               break;
            }
            if (!selected_queue.has_value()) {
               continue;
            }

            PhysicalDeviceSelection candidate{};
            candidate.device = device;
            candidate.queue_family = *selected_queue;
            vkGetPhysicalDeviceProperties(device, &candidate.properties);
            vkGetPhysicalDeviceFeatures(device, &candidate.features);
            for (const char *name : request.required_extensions) {
               appendUniqueName(candidate.enabled_extensions, name);
            }
            appendAvailableOptionalNames(candidate.enabled_extensions,
                                         request.optional_extensions,
                                         extension_span);
#ifdef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
            if (hasExtension(extension_span, VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME)) {
               appendUniqueName(candidate.enabled_extensions, VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
            }
#endif
            candidate.score = deviceTypeScore(candidate.properties.deviceType, request.prefer_discrete_gpu) +
                              static_cast<std::uint32_t>(candidate.enabled_extensions.size() * 10U) +
                              queue_families[*selected_queue].queueCount;

            if (!found || candidate.score > best.score) {
               best = std::move(candidate);
               found = true;
            }
         }

         if (!found) {
            return VK_ERROR_INITIALIZATION_FAILED;
         }

         selection = std::move(best);
         return VK_SUCCESS;
      } catch (const std::bad_alloc &) {
         selection = {};
         return VK_ERROR_OUT_OF_HOST_MEMORY;
      }
   }

   VkResult createDevice(const DeviceProfile &profile, DeviceCreation &creation) noexcept {
      try {
         creation = {};
         if (profile.physical_device == VK_NULL_HANDLE) {
            return VK_ERROR_INITIALIZATION_FAILED;
         }

         std::vector<VkExtensionProperties> available_extensions{};
         if (const VkResult result = enumerateDeviceExtensions(profile.physical_device, available_extensions);
             result != VK_SUCCESS) {
            return result;
         }
         const std::span<const VkExtensionProperties> extension_span{available_extensions};
         if (!hasRequiredNames(profile.required_extensions, extension_span)) {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
         }

         for (const char *name : profile.required_extensions) {
            appendUniqueName(creation.enabled_extensions, name);
         }
         appendAvailableOptionalNames(creation.enabled_extensions, profile.optional_extensions, extension_span);
#ifdef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
         if (hasExtension(extension_span, VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME)) {
            appendUniqueName(creation.enabled_extensions, VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
         }
#endif

         const float queue_priority = 1.0F;
         const auto queue_info = makeDeviceQueueCreateInfo(profile.queue_family, &queue_priority);
         const VkDeviceCreateInfo create_info{.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                                              .pNext = profile.next,
                                              .flags = 0,
                                              .queueCreateInfoCount = 1,
                                              .pQueueCreateInfos = &queue_info,
                                              .enabledLayerCount = static_cast<std::uint32_t>(profile.layers.size()),
                                              .ppEnabledLayerNames = profile.layers.data(),
                                              .enabledExtensionCount =
                                                  static_cast<std::uint32_t>(creation.enabled_extensions.size()),
                                              .ppEnabledExtensionNames = creation.enabled_extensions.data(),
                                              .pEnabledFeatures = profile.features};
         const VkResult result = vkCreateDevice(profile.physical_device, &create_info, nullptr, &creation.device);
         if (result != VK_SUCCESS) {
            creation = {};
            return result;
         }

         vkGetDeviceQueue(creation.device, profile.queue_family, 0, &creation.queue);
         return VK_SUCCESS;
      } catch (const std::bad_alloc &) {
         creation = {};
         return VK_ERROR_OUT_OF_HOST_MEMORY;
      }
   }

   std::optional<std::uint32_t>
   findMemoryType(const VkPhysicalDeviceMemoryProperties &memory_properties,
                  std::uint32_t type_bits,
                  VkMemoryPropertyFlags required_properties) noexcept {
      for (std::uint32_t memory_type = 0; memory_type < memory_properties.memoryTypeCount; ++memory_type) {
         const bool type_supported = (type_bits & (1U << memory_type)) != 0;
         const auto property_flags = memory_properties.memoryTypes[memory_type].propertyFlags;
         if (type_supported && (property_flags & required_properties) == required_properties) {
            return memory_type;
         }
      }

      return std::nullopt;
   }

   VkResult allocateBuffer(const BufferAllocationRequest &request, BufferAllocation &allocation) noexcept {
      allocation = {};
      if (request.physical_device == VK_NULL_HANDLE || request.device == VK_NULL_HANDLE ||
          request.size == 0 || request.usage == 0) {
         return VK_ERROR_INITIALIZATION_FAILED;
      }
      if (!request.initial_data.empty()) {
         if (static_cast<VkDeviceSize>(request.initial_data.size()) > request.size ||
             (request.memory_properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == 0) {
            return VK_ERROR_MEMORY_MAP_FAILED;
         }
      }

      const auto buffer_info = makeBufferCreateInfo(request.size, request.usage);
      VkResult result = vkCreateBuffer(request.device, &buffer_info, nullptr, &allocation.buffer);
      if (result != VK_SUCCESS) {
         allocation = {};
         return result;
      }

      VkMemoryRequirements requirements{};
      vkGetBufferMemoryRequirements(request.device, allocation.buffer, &requirements);

      VkPhysicalDeviceMemoryProperties memory_properties{};
      vkGetPhysicalDeviceMemoryProperties(request.physical_device, &memory_properties);
      const auto memory_type = findMemoryType(memory_properties,
                                              requirements.memoryTypeBits,
                                              request.memory_properties);
      if (!memory_type.has_value()) {
         destroyBuffer(request.device, allocation);
         return VK_ERROR_FEATURE_NOT_PRESENT;
      }

      const VkMemoryAllocateInfo allocate_info{.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                                               .pNext = nullptr,
                                               .allocationSize = requirements.size,
                                               .memoryTypeIndex = *memory_type};
      result = vkAllocateMemory(request.device, &allocate_info, nullptr, &allocation.memory);
      if (result != VK_SUCCESS) {
         destroyBuffer(request.device, allocation);
         return result;
      }

      result = vkBindBufferMemory(request.device, allocation.buffer, allocation.memory, 0);
      if (result != VK_SUCCESS) {
         destroyBuffer(request.device, allocation);
         return result;
      }

      if (!request.initial_data.empty()) {
         void *mapped = nullptr;
         result = vkMapMemory(request.device,
                              allocation.memory,
                              0,
                              static_cast<VkDeviceSize>(request.initial_data.size()),
                              0,
                              &mapped);
         if (result != VK_SUCCESS || mapped == nullptr) {
            destroyBuffer(request.device, allocation);
            return result == VK_SUCCESS ? VK_ERROR_MEMORY_MAP_FAILED : result;
         }

         std::memcpy(mapped, request.initial_data.data(), request.initial_data.size());
         vkUnmapMemory(request.device, allocation.memory);
      }

      allocation.size = request.size;
      return VK_SUCCESS;
   }

   void destroyBuffer(VkDevice device, BufferAllocation &allocation) noexcept {
      if (device != VK_NULL_HANDLE && allocation.buffer != VK_NULL_HANDLE) {
         vkDestroyBuffer(device, allocation.buffer, nullptr);
      }
      if (device != VK_NULL_HANDLE && allocation.memory != VK_NULL_HANDLE) {
         vkFreeMemory(device, allocation.memory, nullptr);
      }
      allocation = {};
   }

   VkResult allocateImage2D(const Image2DAllocationRequest &request, Image2DAllocation &allocation) noexcept {
      allocation = {};
      if (request.physical_device == VK_NULL_HANDLE || request.device == VK_NULL_HANDLE ||
          request.width == 0 || request.height == 0 || request.usage == 0 ||
          request.mip_levels == 0 || request.array_layers == 0) {
         return VK_ERROR_INITIALIZATION_FAILED;
      }

      const auto image_info = makeImageCreateInfo2D(request);
      VkResult result = vkCreateImage(request.device, &image_info, nullptr, &allocation.image);
      if (result != VK_SUCCESS) {
         allocation = {};
         return result;
      }

      VkMemoryRequirements requirements{};
      vkGetImageMemoryRequirements(request.device, allocation.image, &requirements);

      VkPhysicalDeviceMemoryProperties memory_properties{};
      vkGetPhysicalDeviceMemoryProperties(request.physical_device, &memory_properties);
      const auto memory_type = findMemoryType(memory_properties,
                                              requirements.memoryTypeBits,
                                              request.memory_properties);
      if (!memory_type.has_value()) {
         destroyImage2D(request.device, allocation);
         return VK_ERROR_FEATURE_NOT_PRESENT;
      }

      const VkMemoryAllocateInfo allocate_info{.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                                               .pNext = nullptr,
                                               .allocationSize = requirements.size,
                                               .memoryTypeIndex = *memory_type};
      result = vkAllocateMemory(request.device, &allocate_info, nullptr, &allocation.memory);
      if (result != VK_SUCCESS) {
         destroyImage2D(request.device, allocation);
         return result;
      }

      result = vkBindImageMemory(request.device, allocation.image, allocation.memory, 0);
      if (result != VK_SUCCESS) {
         destroyImage2D(request.device, allocation);
         return result;
      }

      if (request.create_view) {
         const auto view_info = makeImageViewCreateInfo2D(allocation.image, request);
         result = vkCreateImageView(request.device, &view_info, nullptr, &allocation.view);
         if (result != VK_SUCCESS) {
            destroyImage2D(request.device, allocation);
            return result;
         }
      }

      if (request.create_sampler) {
         const auto sampler_info = makeLinearRepeatSamplerCreateInfo();
         result = vkCreateSampler(request.device, &sampler_info, nullptr, &allocation.sampler);
         if (result != VK_SUCCESS) {
            destroyImage2D(request.device, allocation);
            return result;
         }
      }

      allocation.width = request.width;
      allocation.height = request.height;
      allocation.format = request.format;
      return VK_SUCCESS;
   }

   void destroyImage2D(VkDevice device, Image2DAllocation &allocation) noexcept {
      if (device != VK_NULL_HANDLE && allocation.sampler != VK_NULL_HANDLE) {
         vkDestroySampler(device, allocation.sampler, nullptr);
      }
      if (device != VK_NULL_HANDLE && allocation.view != VK_NULL_HANDLE) {
         vkDestroyImageView(device, allocation.view, nullptr);
      }
      if (device != VK_NULL_HANDLE && allocation.image != VK_NULL_HANDLE) {
         vkDestroyImage(device, allocation.image, nullptr);
      }
      if (device != VK_NULL_HANDLE && allocation.memory != VK_NULL_HANDLE) {
         vkFreeMemory(device, allocation.memory, nullptr);
      }
      allocation = {};
   }

   VkResult submitOneTimeCommands(const OneTimeSubmitRequest &request) noexcept {
      if (request.device == VK_NULL_HANDLE || request.queue == VK_NULL_HANDLE ||
          request.command_pool == VK_NULL_HANDLE || request.recorder == nullptr) {
         return VK_ERROR_INITIALIZATION_FAILED;
      }

      VkCommandBuffer command_buffer = VK_NULL_HANDLE;
      const VkCommandBufferAllocateInfo allocate_info{
          .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
          .pNext = nullptr,
          .commandPool = request.command_pool,
          .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
          .commandBufferCount = 1};
      VkResult result = vkAllocateCommandBuffers(request.device, &allocate_info, &command_buffer);
      if (result != VK_SUCCESS) {
         return result;
      }

      const auto free_command_buffer = [&request, &command_buffer]() {
         if (command_buffer != VK_NULL_HANDLE) {
            vkFreeCommandBuffers(request.device, request.command_pool, 1, &command_buffer);
            command_buffer = VK_NULL_HANDLE;
         }
      };

      const VkCommandBufferBeginInfo begin_info{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                                .pNext = nullptr,
                                                .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
                                                .pInheritanceInfo = nullptr};
      result = vkBeginCommandBuffer(command_buffer, &begin_info);
      if (result != VK_SUCCESS) {
         free_command_buffer();
         return result;
      }

      result = request.recorder(command_buffer, request.recorder_context);
      if (result != VK_SUCCESS) {
         free_command_buffer();
         return result;
      }

      result = vkEndCommandBuffer(command_buffer);
      if (result != VK_SUCCESS) {
         free_command_buffer();
         return result;
      }

      const VkSubmitInfo submit_info{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                                     .pNext = nullptr,
                                     .waitSemaphoreCount = 0,
                                     .pWaitSemaphores = nullptr,
                                     .pWaitDstStageMask = nullptr,
                                     .commandBufferCount = 1,
                                     .pCommandBuffers = &command_buffer,
                                     .signalSemaphoreCount = 0,
                                     .pSignalSemaphores = nullptr};
      result = vkQueueSubmit(request.queue, 1, &submit_info, VK_NULL_HANDLE);
      if (result == VK_SUCCESS) {
         result = vkQueueWaitIdle(request.queue);
      }
      free_command_buffer();
      return result;
   }

   VkSurfaceFormatKHR chooseSurfaceFormat(std::span<const VkSurfaceFormatKHR> formats) noexcept {
      constexpr VkSurfaceFormatKHR fallback{.format = VK_FORMAT_B8G8R8A8_SRGB,
                                            .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
      constexpr VkFormat preferred_formats[]{
          VK_FORMAT_B8G8R8A8_SRGB,
          VK_FORMAT_R8G8B8A8_SRGB,
          VK_FORMAT_B8G8R8A8_UNORM,
          VK_FORMAT_R8G8B8A8_UNORM,
          VK_FORMAT_R16G16B16A16_SFLOAT};

      if (formats.empty()) {
         return fallback;
      }
      if (formats.size() == 1 && formats.front().format == VK_FORMAT_UNDEFINED) {
         return fallback;
      }

      for (const auto preferred_format : preferred_formats) {
         const auto preferred = std::ranges::find_if(formats, [preferred_format](const VkSurfaceFormatKHR &format) {
            return format.format == preferred_format &&
                   format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
         });
         if (preferred != formats.end()) {
            return *preferred;
         }
      }

      return formats.front();
   }

   VkPresentModeKHR choosePresentMode(std::span<const VkPresentModeKHR> modes) noexcept {
      if (std::ranges::contains(modes, VK_PRESENT_MODE_MAILBOX_KHR)) {
         return VK_PRESENT_MODE_MAILBOX_KHR;
      }
      if (std::ranges::contains(modes, VK_PRESENT_MODE_FIFO_KHR)) {
         return VK_PRESENT_MODE_FIFO_KHR;
      }

      return modes.empty() ? VK_PRESENT_MODE_FIFO_KHR : modes.front();
   }

   VkExtent2D chooseSwapchainExtent(const VkSurfaceCapabilitiesKHR &capabilities,
                                    std::uint32_t requested_width,
                                    std::uint32_t requested_height) noexcept {
      if (capabilities.currentExtent.width != std::numeric_limits<std::uint32_t>::max()) {
         return capabilities.currentExtent;
      }

      const auto clamp_extent = [](std::uint32_t value, std::uint32_t minimum, std::uint32_t maximum) {
         return std::min(std::max(value, minimum), maximum);
      };
      return VkExtent2D{.width = clamp_extent(std::max(requested_width, 1U),
                                              capabilities.minImageExtent.width,
                                              capabilities.maxImageExtent.width),
                        .height = clamp_extent(std::max(requested_height, 1U),
                                               capabilities.minImageExtent.height,
                                               capabilities.maxImageExtent.height)};
   }

   VkCompositeAlphaFlagBitsKHR chooseCompositeAlpha(VkCompositeAlphaFlagsKHR supported) noexcept {
      if ((supported & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) != 0) {
         return VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
      }
      if ((supported & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR) != 0) {
         return VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
      }
      if ((supported & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR) != 0) {
         return VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR;
      }

      return VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
   }

   std::uint32_t frameSyncSlotCount(std::uint32_t swapchain_image_count) noexcept {
      return std::max(1U, std::min(2U, swapchain_image_count));
   }

   std::optional<std::uint32_t> imageFormatBytesPerTexel(VkFormat format) noexcept {
      switch (format) {
      case VK_FORMAT_R8_UNORM:
      case VK_FORMAT_R8_SRGB:
         return 1U;
      case VK_FORMAT_R8G8_UNORM:
      case VK_FORMAT_R8G8_SRGB:
      case VK_FORMAT_D16_UNORM:
         return 2U;
      case VK_FORMAT_R8G8B8_UNORM:
      case VK_FORMAT_R8G8B8_SRGB:
      case VK_FORMAT_B8G8R8_UNORM:
      case VK_FORMAT_B8G8R8_SRGB:
         return 3U;
      case VK_FORMAT_R8G8B8A8_UNORM:
      case VK_FORMAT_R8G8B8A8_SRGB:
      case VK_FORMAT_B8G8R8A8_UNORM:
      case VK_FORMAT_B8G8R8A8_SRGB:
      case VK_FORMAT_D32_SFLOAT:
      case VK_FORMAT_D24_UNORM_S8_UINT:
         return 4U;
      case VK_FORMAT_R16G16B16A16_SFLOAT:
         return 8U;
      case VK_FORMAT_R32G32B32A32_SFLOAT:
         return 16U;
      default:
         return std::nullopt;
      }
   }

} // namespace vh::low
