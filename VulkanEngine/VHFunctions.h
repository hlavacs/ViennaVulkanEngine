#ifndef VHFUNCTIONS_H
#define VHFUNCTIONS_H

#define VK_EXPORTED_FUNCTION(fun) extern PFN_##fun fun;
#define VK_GLOBAL_LEVEL_FUNCTION(fun) extern PFN_##fun fun;
#define VK_INSTANCE_LEVEL_FUNCTION(fun) extern PFN_##fun fun;
#define VK_DEVICE_LEVEL_FUNCTION(fun) extern PFN_##fun fun;

#include "VHFunctions.inl"

VkResult vhLoadVulkanLibrary();

VkResult vhLoadExportedEntryPoints();

VkResult vhLoadGlobalLevelEntryPoints();

VkResult vhLoadInstanceLevelEntryPoints(VkInstance instance);

VkResult vhLoadDeviceLevelEntryPoints(VkInstance instance, VkDevice device);

#endif
