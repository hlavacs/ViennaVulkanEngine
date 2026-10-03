/**
 * @file
 * @brief CPU-only checks of buffer allocation policies without creating a Vulkan device.
 *
 * Functional objects:
 * - main: verifies VMA access flags and required memory properties for each buffer use.
 */
#include <vulkan/vulkan_core.h>
#include <vk_mem_alloc.h>

import std;
import VVEngine.Simple.Vulkan;

/// @brief Checks device-local storage, sequential uploads and cached readback with distinct failure codes.
int main() {
   using vve::simple::BufferMemory;
   using vve::simple::allocationInfoFor;
   constexpr auto host_access = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
      VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT;
   constexpr auto coherent_host = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

   // Static GPU storage must require device locality without requesting host access or a mapping.
   constexpr auto local = allocationInfoFor(BufferMemory::device_local);
   std::println("[VulkanMemoryPolicyTests] device_local flags={} required={}", local.flags, local.requiredFlags);
   if ((local.flags & (host_access | VMA_ALLOCATION_CREATE_MAPPED_BIT)) != 0U ||
       !(local.requiredFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) { return 1; }

   // Sequential coherent writes support staging, uniforms, materials and dynamic vertex slots.
   constexpr auto upload = allocationInfoFor(BufferMemory::upload);
   std::println("[VulkanMemoryPolicyTests] upload flags={} required={}", upload.flags, upload.requiredFlags);
   if ((upload.flags & host_access) != VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT ||
       !(upload.flags & VMA_ALLOCATION_CREATE_MAPPED_BIT) ||
       (upload.requiredFlags & coherent_host) != coherent_host) { return 2; }

   // GPU results require coherent CPU reads; VMA's random-access hint prefers cached memory.
   constexpr auto readback = allocationInfoFor(BufferMemory::readback);
   std::println("[VulkanMemoryPolicyTests] readback flags={} required={}", readback.flags, readback.requiredFlags);
   if ((readback.flags & host_access) != VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT ||
       !(readback.flags & VMA_ALLOCATION_CREATE_MAPPED_BIT) ||
       (readback.requiredFlags & coherent_host) != coherent_host) { return 3; }
   return 0;
}
