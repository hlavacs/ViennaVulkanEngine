module;

#include <vulkan/vulkan.h>

export module vh.low;
import std;

export namespace vh::low {

   struct ApiVersionPolicy {
      std::uint32_t minimum{VK_API_VERSION_1_1};
      std::uint32_t preferred{VK_API_VERSION_1_3};
      std::uint32_t maximum{VK_API_VERSION_1_4};
   };

   struct InstanceProfile {
      const char *application_name{"Vulkan Application"};
      std::uint32_t application_version{VK_MAKE_VERSION(1, 0, 0)};
      const char *engine_name{"vh"};
      std::uint32_t engine_version{VK_MAKE_VERSION(1, 0, 0)};
      ApiVersionPolicy api_version{};
      std::span<const char *const> required_extensions{};
      std::span<const char *const> optional_extensions{};
      std::span<const char *const> required_layers{};
      bool enable_portability_enumeration{true};
      const void *next{nullptr};
   };

   struct InstancePlan {
      std::uint32_t api_version{VK_API_VERSION_1_1};
      VkInstanceCreateFlags flags{0};
      std::vector<const char *> extensions{};
      std::vector<const char *> layers{};
   };

   using PresentationSupportCallback = VkBool32 (*)(VkInstance instance,
                                                    VkPhysicalDevice device,
                                                    std::uint32_t queue_family,
                                                    void *context);

   struct PhysicalDeviceSelectionRequest {
      VkInstance instance{VK_NULL_HANDLE};
      VkQueueFlags required_queue_flags{VK_QUEUE_GRAPHICS_BIT};
      std::span<const char *const> required_extensions{};
      std::span<const char *const> optional_extensions{};
      PresentationSupportCallback presentation_support{nullptr};
      void *presentation_context{nullptr};
      bool prefer_discrete_gpu{true};
   };

   struct PhysicalDeviceSelection {
      VkPhysicalDevice device{VK_NULL_HANDLE};
      std::uint32_t queue_family{0};
      VkPhysicalDeviceProperties properties{};
      VkPhysicalDeviceFeatures features{};
      std::vector<const char *> enabled_extensions{};
      std::uint32_t score{0};
   };

   struct DeviceProfile {
      VkPhysicalDevice physical_device{VK_NULL_HANDLE};
      std::uint32_t queue_family{0};
      std::span<const char *const> required_extensions{};
      std::span<const char *const> optional_extensions{};
      std::span<const char *const> layers{};
      const VkPhysicalDeviceFeatures *features{nullptr};
      const void *next{nullptr};
   };

   struct DeviceCreation {
      VkDevice device{VK_NULL_HANDLE};
      VkQueue queue{VK_NULL_HANDLE};
      std::vector<const char *> enabled_extensions{};
   };

   struct BufferAllocationRequest {
      VkPhysicalDevice physical_device{VK_NULL_HANDLE};
      VkDevice device{VK_NULL_HANDLE};
      VkDeviceSize size{0};
      VkBufferUsageFlags usage{0};
      VkMemoryPropertyFlags memory_properties{VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
      std::span<const std::byte> initial_data{};
   };

   struct BufferAllocation {
      VkBuffer buffer{VK_NULL_HANDLE};
      VkDeviceMemory memory{VK_NULL_HANDLE};
      VkDeviceSize size{0};
   };

   struct Image2DAllocationRequest {
      VkPhysicalDevice physical_device{VK_NULL_HANDLE};
      VkDevice device{VK_NULL_HANDLE};
      std::uint32_t width{0};
      std::uint32_t height{0};
      VkFormat format{VK_FORMAT_R8G8B8A8_UNORM};
      VkImageUsageFlags usage{VK_IMAGE_USAGE_SAMPLED_BIT};
      VkImageAspectFlags aspect_mask{VK_IMAGE_ASPECT_COLOR_BIT};
      VkMemoryPropertyFlags memory_properties{VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT};
      std::uint32_t mip_levels{1};
      std::uint32_t array_layers{1};
      bool create_view{true};
      bool create_sampler{false};
   };

   struct Image2DAllocation {
      VkImage image{VK_NULL_HANDLE};
      VkDeviceMemory memory{VK_NULL_HANDLE};
      VkImageView view{VK_NULL_HANDLE};
      VkSampler sampler{VK_NULL_HANDLE};
      std::uint32_t width{0};
      std::uint32_t height{0};
      VkFormat format{VK_FORMAT_UNDEFINED};
   };

   using CommandRecorder = VkResult (*)(VkCommandBuffer command_buffer, void *context);

   struct OneTimeSubmitRequest {
      VkDevice device{VK_NULL_HANDLE};
      VkQueue queue{VK_NULL_HANDLE};
      VkCommandPool command_pool{VK_NULL_HANDLE};
      CommandRecorder recorder{nullptr};
      void *recorder_context{nullptr};
   };

   [[nodiscard]] std::string versionString(std::uint32_t version);
   [[nodiscard]] std::uint32_t chooseApiVersion(std::uint32_t available_version,
                                                ApiVersionPolicy policy) noexcept;
   [[nodiscard]] VkResult querySupportedApiVersion(ApiVersionPolicy policy,
                                                   std::uint32_t *selected_version) noexcept;

   [[nodiscard]] bool hasName(std::span<const char *const> names, std::string_view name) noexcept;
   void appendUniqueName(std::vector<const char *> &names, const char *name);
   [[nodiscard]] bool hasExtension(std::span<const VkExtensionProperties> extensions,
                                   std::string_view name) noexcept;
   [[nodiscard]] bool hasLayer(std::span<const VkLayerProperties> layers,
                               std::string_view name) noexcept;
   [[nodiscard]] bool isSuccessfulEnumerationResult(VkResult result) noexcept;

   [[nodiscard]] VkResult enumerateInstanceExtensions(std::vector<VkExtensionProperties> &extensions) noexcept;
   [[nodiscard]] VkResult enumerateInstanceLayers(std::vector<VkLayerProperties> &layers) noexcept;
   [[nodiscard]] VkResult planInstance(const InstanceProfile &profile,
                                       InstancePlan &plan) noexcept;
   [[nodiscard]] VkResult createInstance(const InstanceProfile &profile,
                                         InstancePlan *plan,
                                         VkInstance *instance) noexcept;

   [[nodiscard]] VkResult enumeratePhysicalDevices(VkInstance instance,
                                                   std::vector<VkPhysicalDevice> &devices) noexcept;
   [[nodiscard]] VkResult enumerateQueueFamilies(VkPhysicalDevice device,
                                                 std::vector<VkQueueFamilyProperties> &queue_families) noexcept;
   [[nodiscard]] VkResult enumerateDeviceExtensions(VkPhysicalDevice device,
                                                    std::vector<VkExtensionProperties> &extensions) noexcept;
   [[nodiscard]] VkResult selectPhysicalDevice(const PhysicalDeviceSelectionRequest &request,
                                               PhysicalDeviceSelection &selection) noexcept;
   [[nodiscard]] VkResult createDevice(const DeviceProfile &profile,
                                       DeviceCreation &creation) noexcept;

   [[nodiscard]] std::optional<std::uint32_t>
   findMemoryType(const VkPhysicalDeviceMemoryProperties &memory_properties,
                  std::uint32_t type_bits,
                  VkMemoryPropertyFlags required_properties) noexcept;
   [[nodiscard]] VkResult allocateBuffer(const BufferAllocationRequest &request,
                                         BufferAllocation &allocation) noexcept;
   void destroyBuffer(VkDevice device, BufferAllocation &allocation) noexcept;
   [[nodiscard]] VkResult allocateImage2D(const Image2DAllocationRequest &request,
                                          Image2DAllocation &allocation) noexcept;
   void destroyImage2D(VkDevice device, Image2DAllocation &allocation) noexcept;

   [[nodiscard]] VkResult submitOneTimeCommands(const OneTimeSubmitRequest &request) noexcept;

   [[nodiscard]] VkSurfaceFormatKHR chooseSurfaceFormat(std::span<const VkSurfaceFormatKHR> formats) noexcept;
   [[nodiscard]] VkPresentModeKHR choosePresentMode(std::span<const VkPresentModeKHR> modes) noexcept;
   [[nodiscard]] VkExtent2D chooseSwapchainExtent(const VkSurfaceCapabilitiesKHR &capabilities,
                                                  std::uint32_t requested_width,
                                                  std::uint32_t requested_height) noexcept;
   [[nodiscard]] VkCompositeAlphaFlagBitsKHR chooseCompositeAlpha(VkCompositeAlphaFlagsKHR supported) noexcept;
   [[nodiscard]] std::uint32_t frameSyncSlotCount(std::uint32_t swapchain_image_count) noexcept;
   [[nodiscard]] std::optional<std::uint32_t> imageFormatBytesPerTexel(VkFormat format) noexcept;

} // namespace vh::low
