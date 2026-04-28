#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>
#include <vulkan/vulkan.h>

import vh.low;

/**
 * @file
 * @brief Regression tests for the stateless Vulkan helper layer.
 */
int main() {
   if (vh::low::versionString(VK_MAKE_VERSION(1, 2, 3)) != "1.2.3") {
      return 1;
   }
   if (vh::low::chooseApiVersion(VK_API_VERSION_1_2,
                                 vh::low::ApiVersionPolicy{.minimum = VK_API_VERSION_1_1,
                                                           .preferred = VK_API_VERSION_1_3,
                                                           .maximum = VK_API_VERSION_1_4}) != VK_API_VERSION_1_2) {
      return 13;
   }
   if (vh::low::chooseApiVersion(VK_API_VERSION_1_0,
                                 vh::low::ApiVersionPolicy{.minimum = VK_API_VERSION_1_1}) != 0U) {
      return 14;
   }

   std::vector<const char *> names{};
   vh::low::appendUniqueName(names, VK_KHR_SURFACE_EXTENSION_NAME);
   vh::low::appendUniqueName(names, VK_KHR_SURFACE_EXTENSION_NAME);
   if (names.size() != 1 ||
       !vh::low::hasName(std::span<const char *const>{names.data(), names.size()},
                         VK_KHR_SURFACE_EXTENSION_NAME)) {
      return 2;
   }

   std::array<VkExtensionProperties, 1> extensions{};
   std::string_view{VK_KHR_SWAPCHAIN_EXTENSION_NAME}.copy(extensions.front().extensionName,
                                                          VK_MAX_EXTENSION_NAME_SIZE - 1);
   if (!vh::low::hasExtension(std::span<const VkExtensionProperties>{extensions},
                              VK_KHR_SWAPCHAIN_EXTENSION_NAME)) {
      return 3;
   }

   const std::array formats{
       VkSurfaceFormatKHR{.format = VK_FORMAT_R8G8B8A8_UNORM,
                          .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
       VkSurfaceFormatKHR{.format = VK_FORMAT_B8G8R8A8_SRGB,
                          .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}};
   const auto format = vh::low::chooseSurfaceFormat(std::span<const VkSurfaceFormatKHR>{formats});
   if (format.format != VK_FORMAT_B8G8R8A8_SRGB) {
      return 4;
   }

   const std::array modes{VK_PRESENT_MODE_FIFO_KHR, VK_PRESENT_MODE_MAILBOX_KHR};
   if (vh::low::choosePresentMode(std::span<const VkPresentModeKHR>{modes}) != VK_PRESENT_MODE_MAILBOX_KHR) {
      return 5;
   }

   VkSurfaceCapabilitiesKHR capabilities{};
   capabilities.currentExtent = {.width = UINT32_MAX, .height = UINT32_MAX};
   capabilities.minImageExtent = {.width = 320, .height = 200};
   capabilities.maxImageExtent = {.width = 1920, .height = 1080};
   const auto extent = vh::low::chooseSwapchainExtent(capabilities, 100U, 1200U);
   if (extent.width != 320U || extent.height != 1080U) {
      return 6;
   }

   if (vh::low::chooseCompositeAlpha(VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR) !=
       VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR) {
      return 7;
   }
   if (vh::low::frameSyncSlotCount(0) != 1 || vh::low::frameSyncSlotCount(3) != 2) {
      return 8;
   }

   VkPhysicalDeviceMemoryProperties memory_properties{};
   memory_properties.memoryTypeCount = 2;
   memory_properties.memoryTypes[0].propertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
   memory_properties.memoryTypes[1].propertyFlags =
       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
   const auto memory_type = vh::low::findMemoryType(memory_properties,
                                                    0b10U,
                                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
   if (!memory_type || *memory_type != 1U) {
      return 9;
   }

   if (vh::low::imageFormatBytesPerTexel(VK_FORMAT_R8G8B8A8_SRGB).value_or(0U) != 4U ||
       vh::low::imageFormatBytesPerTexel(VK_FORMAT_UNDEFINED).has_value()) {
      return 10;
   }
   if (vh::low::mipLevelCount2D(1, 1) != 1 || vh::low::mipLevelCount2D(2, 2) != 2 ||
       vh::low::mipLevelCount2D(8, 4) != 4 || vh::low::mipLevelCount2D(0, 4) != 0) {
      return 21;
   }

   vh::low::BufferAllocation allocation{};
   const auto buffer_result = vh::low::allocateBuffer(vh::low::BufferAllocationRequest{
                                                         .physical_device = VK_NULL_HANDLE,
                                                         .device = VK_NULL_HANDLE,
                                                         .size = 64,
                                                         .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT},
                                                      allocation);
   if (buffer_result != VK_ERROR_INITIALIZATION_FAILED || allocation.buffer != VK_NULL_HANDLE) {
      return 11;
   }

   vh::low::Image2DAllocation image{};
   const auto image_result = vh::low::allocateImage2D(vh::low::Image2DAllocationRequest{
                                                         .physical_device = VK_NULL_HANDLE,
                                                         .device = VK_NULL_HANDLE,
                                                         .width = 4,
                                                         .height = 8,
                                                         .format = VK_FORMAT_R8G8B8A8_UNORM,
                                                         .usage = VK_IMAGE_USAGE_SAMPLED_BIT},
                                                      image);
   if (image_result != VK_ERROR_INITIALIZATION_FAILED || image.image != VK_NULL_HANDLE) {
      return 12;
   }

   const auto submit_result = vh::low::submitOneTimeCommands(vh::low::OneTimeSubmitRequest{});
   if (submit_result != VK_ERROR_INITIALIZATION_FAILED) {
      return 15;
   }

   vh::low::SwapchainCreation swapchain{};
   const auto swapchain_result =
       vh::low::createSwapchain(vh::low::SwapchainRequest{}, swapchain);
   if (swapchain_result != VK_ERROR_INITIALIZATION_FAILED ||
       swapchain.swapchain != VK_NULL_HANDLE) {
      return 16;
   }

   std::vector<VkCommandBuffer> command_buffers{};
   const auto command_buffer_result =
       vh::low::allocateCommandBuffers(vh::low::CommandBufferAllocationRequest{}, command_buffers);
   if (command_buffer_result != VK_ERROR_INITIALIZATION_FAILED ||
       !command_buffers.empty()) {
      return 17;
   }

   VkDescriptorSetLayout descriptor_set_layout = VK_NULL_HANDLE;
   const auto descriptor_layout_result =
       vh::low::createDescriptorSetLayout(VK_NULL_HANDLE, {}, descriptor_set_layout);
   if (descriptor_layout_result != VK_ERROR_INITIALIZATION_FAILED ||
       descriptor_set_layout != VK_NULL_HANDLE) {
      return 18;
   }

   const auto upload_recording_result =
       vh::low::recordBufferToImageUpload2D(VK_NULL_HANDLE, vh::low::BufferToImageUpload2DRecording{});
   if (upload_recording_result != VK_ERROR_INITIALIZATION_FAILED) {
      return 19;
   }

   const auto clear_recording_result =
       vh::low::recordClearColorImage(VK_NULL_HANDLE, vh::low::ClearColorImageRecording{});
   if (clear_recording_result != VK_ERROR_INITIALIZATION_FAILED) {
      return 20;
   }

   return 0;
}
