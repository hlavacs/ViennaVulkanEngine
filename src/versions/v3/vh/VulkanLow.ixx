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

   enum class PhysicalDeviceSelectionMode {
      first_compatible,
      highest_score
   };

   struct PhysicalDeviceSelectionRequest {
      VkInstance instance{VK_NULL_HANDLE};
      VkQueueFlags required_queue_flags{VK_QUEUE_GRAPHICS_BIT};
      std::span<const char *const> required_extensions{};
      std::span<const char *const> optional_extensions{};
      PresentationSupportCallback presentation_support{nullptr};
      void *presentation_context{nullptr};
      PhysicalDeviceSelectionMode selection_mode{PhysicalDeviceSelectionMode::first_compatible};
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
      VkSamplerMipmapMode sampler_mipmap_mode{VK_SAMPLER_MIPMAP_MODE_NEAREST};
      VkBool32 sampler_anisotropy_enable{VK_FALSE};
      float sampler_max_anisotropy{1.0F};
      float sampler_min_lod{0.0F};
      float sampler_max_lod{0.0F};
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

   struct SurfaceSupport {
      VkSurfaceCapabilitiesKHR capabilities{};
      std::vector<VkSurfaceFormatKHR> formats{};
      std::vector<VkPresentModeKHR> present_modes{};
   };

   struct SwapchainRequest {
      VkPhysicalDevice physical_device{VK_NULL_HANDLE};
      VkDevice device{VK_NULL_HANDLE};
      VkSurfaceKHR surface{VK_NULL_HANDLE};
      std::uint32_t width{0};
      std::uint32_t height{0};
      VkImageUsageFlags image_usage{VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT};
      VkCompositeAlphaFlagBitsKHR composite_alpha{VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR};
      VkSwapchainKHR old_swapchain{VK_NULL_HANDLE};
   };

   struct SwapchainCreation {
      VkSwapchainKHR swapchain{VK_NULL_HANDLE};
      VkSurfaceFormatKHR surface_format{};
      VkPresentModeKHR present_mode{VK_PRESENT_MODE_FIFO_KHR};
      VkExtent2D extent{};
      std::vector<VkImage> images{};
   };

   struct ImageView2DRequest {
      VkDevice device{VK_NULL_HANDLE};
      std::span<const VkImage> images{};
      VkFormat format{VK_FORMAT_UNDEFINED};
      VkImageAspectFlags aspect_mask{VK_IMAGE_ASPECT_COLOR_BIT};
      std::uint32_t mip_levels{1};
      std::uint32_t array_layers{1};
   };

   struct ColorDepthRenderPassRequest {
      VkDevice device{VK_NULL_HANDLE};
      std::span<const VkFormat> color_formats{};
      VkFormat depth_format{VK_FORMAT_UNDEFINED};
      VkAttachmentLoadOp color_load_op{VK_ATTACHMENT_LOAD_OP_DONT_CARE};
      VkAttachmentStoreOp color_store_op{VK_ATTACHMENT_STORE_OP_STORE};
      VkImageLayout color_initial_layout{VK_IMAGE_LAYOUT_UNDEFINED};
      VkImageLayout color_final_layout{VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
      VkAttachmentLoadOp depth_load_op{VK_ATTACHMENT_LOAD_OP_DONT_CARE};
      VkAttachmentStoreOp depth_store_op{VK_ATTACHMENT_STORE_OP_DONT_CARE};
      VkImageLayout depth_initial_layout{VK_IMAGE_LAYOUT_UNDEFINED};
      VkImageLayout depth_final_layout{VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
      bool external_dependencies{false};
   };

   struct FramebufferRequest {
      VkDevice device{VK_NULL_HANDLE};
      VkRenderPass render_pass{VK_NULL_HANDLE};
      std::span<const VkImageView> color_image_views{};
      VkImageView depth_image_view{VK_NULL_HANDLE};
      VkExtent2D extent{};
      std::uint32_t layers{1};
   };

   struct CommandBufferAllocationRequest {
      VkDevice device{VK_NULL_HANDLE};
      VkCommandPool command_pool{VK_NULL_HANDLE};
      std::uint32_t count{0};
      VkCommandBufferLevel level{VK_COMMAND_BUFFER_LEVEL_PRIMARY};
   };

   struct FrameSyncPrimitives {
      VkSemaphore image_available{VK_NULL_HANDLE};
      VkSemaphore render_finished{VK_NULL_HANDLE};
      VkFence render_fence{VK_NULL_HANDLE};
   };

   struct DescriptorSetAllocationRequest {
      VkDevice device{VK_NULL_HANDLE};
      std::span<const VkDescriptorSetLayout> set_layouts{};
      std::span<const VkDescriptorPoolSize> pool_sizes{};
      std::uint32_t max_sets{0};
      VkDescriptorPoolCreateFlags pool_flags{0};
   };

   struct DescriptorSetAllocation {
      VkDescriptorPool pool{VK_NULL_HANDLE};
      std::vector<VkDescriptorSet> sets{};
   };

   struct BufferToImageUpload2DRecording {
      VkBuffer staging_buffer{VK_NULL_HANDLE};
      VkImage image{VK_NULL_HANDLE};
      std::uint32_t width{0};
      std::uint32_t height{0};
      VkImageAspectFlags aspect_mask{VK_IMAGE_ASPECT_COLOR_BIT};
      std::uint32_t mip_levels{1};
      std::uint32_t array_layers{1};
      VkImageLayout old_layout{VK_IMAGE_LAYOUT_UNDEFINED};
      VkImageLayout transfer_layout{VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL};
      VkImageLayout final_layout{VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
      VkPipelineStageFlags final_dst_stage_mask{VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
                                                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT};
      std::span<const VkBufferImageCopy> copy_regions{};
   };

   struct ClearColorImageRecording {
      VkImage image{VK_NULL_HANDLE};
      VkClearColorValue clear_color{};
      VkImageAspectFlags aspect_mask{VK_IMAGE_ASPECT_COLOR_BIT};
      VkImageLayout old_layout{VK_IMAGE_LAYOUT_UNDEFINED};
      VkImageLayout transfer_layout{VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL};
      VkImageLayout final_layout{VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
      VkPipelineStageFlags final_dst_stage_mask{VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
                                                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT};
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
   [[nodiscard]] VkResult hasDeviceExtension(VkPhysicalDevice device,
                                             std::string_view name,
                                             bool &supported) noexcept;
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
   [[nodiscard]] std::uint32_t mipLevelCount2D(std::uint32_t width, std::uint32_t height) noexcept;
   [[nodiscard]] VkResult allocateImage2D(const Image2DAllocationRequest &request,
                                          Image2DAllocation &allocation) noexcept;
   void destroyImage2D(VkDevice device, Image2DAllocation &allocation) noexcept;

   [[nodiscard]] VkResult submitOneTimeCommands(const OneTimeSubmitRequest &request) noexcept;

   [[nodiscard]] VkResult querySurfaceSupport(VkPhysicalDevice physical_device,
                                              VkSurfaceKHR surface,
                                              SurfaceSupport &support) noexcept;
   [[nodiscard]] VkResult createSwapchain(const SwapchainRequest &request,
                                          SwapchainCreation &creation) noexcept;
   void destroySwapchain(VkDevice device, SwapchainCreation &creation) noexcept;
   [[nodiscard]] VkResult createImageViews2D(const ImageView2DRequest &request,
                                             std::vector<VkImageView> &image_views) noexcept;
   void destroyImageViews(VkDevice device, std::vector<VkImageView> &image_views) noexcept;
   [[nodiscard]] VkResult chooseSupportedDepthFormat(VkPhysicalDevice physical_device,
                                                     std::span<const VkFormat> candidates,
                                                     VkFormatFeatureFlags required_features,
                                                     VkFormat &format) noexcept;
   [[nodiscard]] VkResult createColorDepthRenderPass(const ColorDepthRenderPassRequest &request,
                                                     VkRenderPass &render_pass) noexcept;
   [[nodiscard]] VkResult createFramebuffers(const FramebufferRequest &request,
                                             std::vector<VkFramebuffer> &framebuffers) noexcept;
   void destroyFramebuffers(VkDevice device, std::vector<VkFramebuffer> &framebuffers) noexcept;
   [[nodiscard]] VkResult allocateCommandBuffers(const CommandBufferAllocationRequest &request,
                                                 std::vector<VkCommandBuffer> &command_buffers) noexcept;
   [[nodiscard]] VkResult createFrameSyncPrimitives(VkDevice device,
                                                    std::uint32_t count,
                                                    VkFenceCreateFlags fence_flags,
                                                    std::vector<FrameSyncPrimitives> &sync) noexcept;
   void destroyFrameSyncPrimitives(VkDevice device,
                                   std::vector<FrameSyncPrimitives> &sync) noexcept;
   [[nodiscard]] VkResult createDescriptorSetLayout(VkDevice device,
                                                    std::span<const VkDescriptorSetLayoutBinding> bindings,
                                                    VkDescriptorSetLayout &layout,
                                                    VkDescriptorSetLayoutCreateFlags flags = 0) noexcept;
   [[nodiscard]] VkResult createDescriptorPoolAndAllocateSets(
       const DescriptorSetAllocationRequest &request,
       DescriptorSetAllocation &allocation) noexcept;
   void destroyDescriptorSetAllocation(VkDevice device, DescriptorSetAllocation &allocation) noexcept;
   [[nodiscard]] VkResult allocateDescriptorSets(VkDevice device,
                                                 VkDescriptorPool pool,
                                                 std::span<const VkDescriptorSetLayout> set_layouts,
                                                 std::vector<VkDescriptorSet> &sets) noexcept;
   [[nodiscard]] VkResult recordBufferToImageUpload2D(VkCommandBuffer command_buffer,
                                                      const BufferToImageUpload2DRecording &recording) noexcept;
   [[nodiscard]] VkResult recordClearColorImage(VkCommandBuffer command_buffer,
                                                const ClearColorImageRecording &recording) noexcept;

   [[nodiscard]] VkSurfaceFormatKHR chooseSurfaceFormat(std::span<const VkSurfaceFormatKHR> formats) noexcept;
   [[nodiscard]] VkPresentModeKHR choosePresentMode(std::span<const VkPresentModeKHR> modes) noexcept;
   [[nodiscard]] VkExtent2D chooseSwapchainExtent(const VkSurfaceCapabilitiesKHR &capabilities,
                                                  std::uint32_t requested_width,
                                                  std::uint32_t requested_height) noexcept;
   [[nodiscard]] VkCompositeAlphaFlagBitsKHR chooseCompositeAlpha(VkCompositeAlphaFlagsKHR supported) noexcept;
   [[nodiscard]] std::uint32_t frameSyncSlotCount(std::uint32_t swapchain_image_count) noexcept;
   [[nodiscard]] std::optional<std::uint32_t> imageFormatBytesPerTexel(VkFormat format) noexcept;

} // namespace vh::low
