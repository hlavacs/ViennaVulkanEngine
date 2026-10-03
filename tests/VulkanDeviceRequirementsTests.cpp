/**
 * @file
 * @brief CPU-only coverage of the simple renderer's Vulkan 1.3 device requirements.
 *
 * Functional objects:
 * - main: evaluates synthetic capabilities without creating a Vulkan instance, device or window.
 */
#include <vulkan/vulkan_core.h>

import std;
import VVEngine.Simple.Vulkan;

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
   return missing_swapchain == VK_ERROR_FEATURE_NOT_PRESENT ? 0 : 5;
}
