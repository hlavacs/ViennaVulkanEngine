/**
 * @file
 * @brief Mip policy, GPU blit contents and shared material sampler lifetime coverage.
 *
 * Functional objects:
 * - hasFilteredMip checks rectangular sRGB and linear textures down to their final texel.
 * - main checks feature fallbacks and enabled anisotropy, including validation through cleanup.
 */
#include <vulkan/vulkan_core.h>

import std;
import VVE.TestSupport;
import VEEngine.Simple;
import VEEngine.Simple.Renderer;
import VEEngine.Simple.Vulkan;

/// @brief Checks the final mip's pixels, so allocating levels without generating their contents cannot pass.
[[nodiscard]] bool hasFilteredMip(vve::simple::ForwardRenderer &renderer, VkFormat format, int expected) {
   constexpr VkExtent2D extent{8U, 2U};
   std::array<std::byte, extent.width * extent.height * 4U> pixels{};
   // Equal red and green halves converge to their linear-light average at the final mip.
   for (std::size_t pixel{}; pixel < pixels.size() / 4U; ++pixel) {
      pixels[pixel * 4U + (pixel % extent.width < extent.width / 2U ? 0U : 1U)] = std::byte{255U};
      pixels[pixel * 4U + 3U] = std::byte{255U};
   }
   vve::simple::TextureImage texture{};
   // Staging belongs to the batch; leaving this scope releases it only after the fence wait.
   {
      vve::simple::VulkanUploadBatch batch{};
      if (batch.begin(renderer.device.device, renderer.device.graphicsQueue, renderer.commandPool.commandPool) != VK_SUCCESS ||
          texture.create(renderer.allocator, renderer.device.device, renderer.physicalDevice.physicalDevice,
             batch, pixels, extent, format) != VK_SUCCESS || batch.submit() != VK_SUCCESS) { return false; }
   }
   if (texture.mipLevels != 4U) { return false; }
   vve::simple::VulkanBuffer readback{};
   if (readback.create(renderer.allocator, 4U, VK_BUFFER_USAGE_TRANSFER_DST_BIT, vve::simple::BufferMemory::readback) != VK_SUCCESS) { return false; }
   const auto result = vve::simple::submitOnce(renderer.device.device, renderer.device.graphicsQueue, renderer.commandPool.commandPool,
      [&](VkCommandBuffer command) {
         vve::simple::transitionImage(command, texture.image, texture.aspect, 0U, 1U,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, texture.mipLevels - 1U);
         const VkBufferImageCopy copy{
            .imageSubresource = {.aspectMask = texture.aspect, .mipLevel = texture.mipLevels - 1U, .layerCount = 1U},
            .imageExtent = {1U, 1U, 1U}};
         vkCmdCopyImageToBuffer(command, texture.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readback.buffer, 1U, &copy);
         const VkMemoryBarrier barrier{.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT, .dstAccessMask = VK_ACCESS_HOST_READ_BIT};
         vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0U, 1U, &barrier, 0U, nullptr, 0U, nullptr);
      });
   if (result != VK_SUCCESS) { return false; }
   const auto bytes = readback.bytes();
   std::println("R6 mip format={} levels={} rgba={},{},{},{}", static_cast<int>(format), texture.mipLevels,
      std::to_integer<int>(bytes[0]), std::to_integer<int>(bytes[1]), std::to_integer<int>(bytes[2]), std::to_integer<int>(bytes[3]));
   return std::abs(std::to_integer<int>(bytes[0]) - expected) <= 2 &&
      std::abs(std::to_integer<int>(bytes[1]) - expected) <= 2 && bytes[2] == std::byte{0U} && bytes[3] == std::byte{255U};
}

/// @brief Verifies full chains, each missing format capability, shared sampler lifetime and optional anisotropy.
int main() {
   using vve::simple::textureMipLevels;
   constexpr auto features = VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT | VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT;
   if (textureMipLevels({1U, 1U}, features) != 1U || textureMipLevels({8U, 2U}, features) != 4U ||
       textureMipLevels({3U, 9U}, features) != 4U || textureMipLevels({1024U, 1U}, features) != 11U) { return 1; }
   // Every required flag independently gates blit generation; unsupported formats retain only level zero.
   for (const auto missing : {VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT, VK_FORMAT_FEATURE_BLIT_SRC_BIT, VK_FORMAT_FEATURE_BLIT_DST_BIT}) {
      if (textureMipLevels({8U, 2U}, features & ~missing) != 1U) { return 2; }
   }
   auto engine = vve::test::hiddenEngine("vulkan-texture-tests", vve::PixelExtent{64U, 64U});
   if (!engine.init() || !engine.renderFrame()) { return 3; }
   auto &renderer = engine.renderSystem().forward();
   VkPhysicalDeviceFeatures supported{};
   vkGetPhysicalDeviceFeatures(renderer.physicalDevice.physicalDevice, &supported);
   const auto sampler = renderer.materialSampler;
   if (sampler == VK_NULL_HANDLE || renderer.device.samplerAnisotropy != (supported.samplerAnisotropy == VK_TRUE) ||
       renderer.defaultObjectTexture.mipLevels != 1U) { return 4; }
   if (!hasFilteredMip(renderer, VK_FORMAT_R8G8B8A8_SRGB, 188) ||
       !hasFilteredMip(renderer, VK_FORMAT_R8G8B8A8_UNORM, 128)) { return 5; }
   if (!engine.renderFrame() || renderer.materialSampler != sampler) { return 6; }
   engine.renderSystem().waitIdle();
   engine.gui().shutdownVulkan();
   engine.renderSystem().shutdown();
   std::println("R6 cleanup sampler_released={} validation_errors={}", renderer.materialSampler == VK_NULL_HANDLE, renderer.validationErrorCount());
   return renderer.materialSampler == VK_NULL_HANDLE && renderer.validationErrorCount() == 0U ? 0 : 7;
}
