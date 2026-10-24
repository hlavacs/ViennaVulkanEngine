/**
 * @file
 * @brief CPU-only coverage of Vulkan 1.3 device requirements and material descriptor capacity.
 *
 * Functional objects:
 * - main: evaluates synthetic capabilities without creating a Vulkan instance, device or window.
 */
#include <vulkan/vulkan_core.h>

import std;
import VVEngine.Simple.Vulkan;

/// @brief Checks each device limit independently, including exhausted limits and capacities above the former fixed ceiling.
[[nodiscard]] bool hasDeviceTextureCapacity() {
   constexpr VkPhysicalDeviceLimits limits{
      .maxPerStageDescriptorSamplers = 256U, .maxPerStageDescriptorSampledImages = 256U,
      .maxPerStageResources = 512U, .maxDescriptorSetSamplers = 256U, .maxDescriptorSetSampledImages = 256U};
   const auto capacity = vve::simple::materialTextureCapacity(limits);
   std::println("[VulkanDeviceRequirementsTests] texture_capacity={} expected=253", capacity);
   if (capacity != 253U) { return false; }
   // Sampler and sampled-image limits apply at both stage and descriptor-set scope.
   for (const auto member : {&VkPhysicalDeviceLimits::maxPerStageDescriptorSamplers,
      &VkPhysicalDeviceLimits::maxDescriptorSetSamplers, &VkPhysicalDeviceLimits::maxPerStageDescriptorSampledImages,
      &VkPhysicalDeviceLimits::maxDescriptorSetSampledImages}) {
      for (const auto [value, expected] : std::array{std::pair{32U, 29U}, std::pair{4U, 1U},
         std::pair{3U, 0U}, std::pair{2U, 0U}, std::pair{0U, 0U}}) {
         auto reduced = limits;
         reduced.*member = value;
         if (vve::simple::materialTextureCapacity(reduced) != expected) { return false; }
      }
   }
   // Stage resource accounting also reserves the two buffers and the forward color attachment.
   for (const auto [value, expected] : std::array{std::pair{32U, 26U}, std::pair{7U, 1U},
      std::pair{6U, 0U}, std::pair{5U, 0U}, std::pair{0U, 0U}}) {
      auto reduced = limits;
      reduced.maxPerStageResources = value;
      if (vve::simple::materialTextureCapacity(reduced) != expected) { return false; }
   }
   // The selected material count changes without dropping shadow bindings or buffer visibility.
   for (const auto selected : {1U, 29U, capacity}) {
      const auto bindings = vve::simple::descriptorSetBindings(selected);
      const auto &textures = bindings[vve::simple::shaderBinding::baseColorTextures];
      if (textures.descriptorCount != selected || textures.descriptorType != VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
         textures.stageFlags != VK_SHADER_STAGE_FRAGMENT_BIT) { return false; }
      for (const auto shadow : {vve::simple::shaderBinding::spotShadowArray,
         vve::simple::shaderBinding::dirShadowArray, vve::simple::shaderBinding::pointShadowArray}) {
         if (bindings[shadow].binding != shadow || bindings[shadow].descriptorCount != 1U ||
            bindings[shadow].descriptorType != VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) { return false; }
      }
      // Frame uniforms are also read by the terrain tessellation stages, the height grid only by the domain stage.
      if (bindings[vve::simple::shaderBinding::frameUniforms].stageFlags !=
         (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT |
          VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT | VK_SHADER_STAGE_FRAGMENT_BIT) ||
         bindings[vve::simple::shaderBinding::materials].stageFlags != VK_SHADER_STAGE_FRAGMENT_BIT) { return false; }
      const auto &heights = bindings[vve::simple::shaderBinding::terrainHeights];
      if (heights.binding != vve::simple::shaderBinding::terrainHeights || heights.descriptorCount != 1U ||
         heights.descriptorType != VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ||
         heights.stageFlags != VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT) { return false; }
   }
   std::println("[VulkanDeviceRequirementsTests] descriptor_limits=4 resource_budget=true exhausted_limits=0");
   return true;
}

/// @brief Reports each required capability independently; returns a distinct failure code for each case.
int main() {
   constexpr vve::simple::DeviceCapabilities supported{
      .apiVersion = VK_API_VERSION_1_3,
      .swapchainExtension = true,
      .dynamicRendering = true,
      .shaderSampledImageArrayDynamicIndexing = true,
   };
   // Successful evaluation needs only capabilities, never a loader call or a physical device.
   const auto accepted = vve::simple::evaluateDeviceRequirements(supported);
   std::println("[VulkanDeviceRequirementsTests] case=vulkan_1_3 result={}", static_cast<int>(accepted));
   if (accepted != VK_SUCCESS) { return 1; }

   // VK_KHR_dynamic_rendering on Vulkan 1.2 can expose the feature but cannot supply the required core 1.3 contract.
   auto older = supported;
   older.apiVersion = VK_API_VERSION_1_2;
   const auto old_api = vve::simple::evaluateDeviceRequirements(older);
   std::println("[VulkanDeviceRequirementsTests] case=vulkan_1_2_with_KHR_dynamic_rendering result={}", static_cast<int>(old_api));
   if (old_api != VK_ERROR_FEATURE_NOT_PRESENT) { return 2; }

   auto no_rendering = supported;
   no_rendering.dynamicRendering = false;
   const auto missing_rendering = vve::simple::evaluateDeviceRequirements(no_rendering);
   std::println("[VulkanDeviceRequirementsTests] case=missing_dynamic_rendering result={}", static_cast<int>(missing_rendering));
   if (missing_rendering != VK_ERROR_FEATURE_NOT_PRESENT) { return 3; }

   auto no_indexing = supported;
   no_indexing.shaderSampledImageArrayDynamicIndexing = false;
   const auto missing_indexing = vve::simple::evaluateDeviceRequirements(no_indexing);
   std::println("[VulkanDeviceRequirementsTests] case=missing_dynamic_indexing result={}", static_cast<int>(missing_indexing));
   if (missing_indexing != VK_ERROR_FEATURE_NOT_PRESENT) { return 4; }

   auto no_swapchain = supported;
   no_swapchain.swapchainExtension = false;
   const auto missing_swapchain = vve::simple::evaluateDeviceRequirements(no_swapchain);
   std::println("[VulkanDeviceRequirementsTests] case=missing_swapchain result={}", static_cast<int>(missing_swapchain));
   if (missing_swapchain != VK_ERROR_FEATURE_NOT_PRESENT) { return 5; }
   return hasDeviceTextureCapacity() ? 0 : 6;
}
