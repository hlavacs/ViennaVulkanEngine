#pragma once

#include <vector>
#include <assert.h>

//#include <vulkan/vulkan.h>
#include "volk.h"
#include "vma/vk_mem_alloc.h"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"

namespace vh {



	//use this macro to check the function result, if its not VK_SUCCESS then return the error
    #define VHCHECKRESULT(x) { CheckResult(VkResult err) };

    void CheckResult(VkResult err);

    //instance and device
    bool IsExtensionAvailable(const std::vector<VkExtensionProperties>& properties, const char* extension);
    auto SelectPhysicalDevice() ;    
    void SetupInstance(std::vector<const char*> layers, std::vector<const char*> instance_extensions, VkAllocationCallbacks* allocator, VkInstance* instance);
    void SetupDebugReport(VkInstance instance, VkAllocationCallbacks* allocator, VkDebugReportCallbackEXT* debugReport);
    void SetupPhysicalDevice(VkInstance instance, std::vector<const char*> device_extensions, VkPhysicalDevice* physicalDevice);
    void SetupGraphicsQueueFamily( VkPhysicalDevice physicalDevice, uint32_t* queueFamily);
    void SetupDevice(   VkPhysicalDevice physicalDevice, VkAllocationCallbacks* allocator, 
                        std::vector<const char*>& device_extensions, uint32_t queueFamily, VkDevice* device);

    void CreateDescriptorPool(VkDevice device, VkDescriptorPool* descriptorPool);
    
    //surface
    

    VkSurfaceFormatKHR SelectSurfaceFormat(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, std::vector<VkFormat> requestSurfaceImageFormat);
    VkPresentModeKHR SelectPresentMode(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, std::vector<VkPresentModeKHR> requestPresentModes);

    //-------------------------------------------------------------------------
    //IMGUI 

    //swapchain

    void DestroyFrame(VkDevice device, ImGui_ImplVulkanH_Frame* fd, const VkAllocationCallbacks* allocator);
    void DestroyFrameSemaphores(VkDevice device, ImGui_ImplVulkanH_FrameSemaphores* fsd, const VkAllocationCallbacks* allocator);

    void CreateWindowSwapChain(VkPhysicalDevice physical_device, VkDevice device, ImGui_ImplVulkanH_Window* wd
        , const VkAllocationCallbacks* allocator, int w, int h, uint32_t min_image_count);

    void CreateWindowCommandBuffers(VkPhysicalDevice physical_device, VkDevice device
        , ImGui_ImplVulkanH_Window* wd, uint32_t queue_family, const VkAllocationCallbacks* allocator);



    //-------------------------------------------------------------------------

    //-------------------------------------------------------------------------

    //-------------------------------------------------------------------------


	///need only for start up
	struct QueueFamilyIndices
	{
		int graphicsFamily = -1; ///<Index of graphics family
		int presentFamily = -1; ///<Index of present family

		///\returns true if the structure is filled completely
		bool isComplete()
		{
			return graphicsFamily >= 0 && presentFamily >= 0;
		}
	};

	///need only for start up
	struct SwapChainSupportDetails
	{
		VkSurfaceCapabilitiesKHR capabilities; ///<Surface capabilities
		std::vector<VkSurfaceFormatKHR> formats; ///<Surface formats available
		std::vector<VkPresentModeKHR> presentModes; ///<Possible present modes
	};


	VkResult vhDevCreateInstance(std::vector<const char *> &extensions
		, std::vector<const char *> &validationLayers, VkInstance *instance);

	QueueFamilyIndices vhDevFindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);

	bool isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface, std::vector<const char *> requiredDeviceExtensions);

	VkResult vhDevPickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface, std::vector<const char *> requiredDeviceExtensions
		, VkPhysicalDevice *physicalDevice, VkPhysicalDeviceFeatures *pFeatures, VkPhysicalDeviceLimits *limits);

	bool checkDeviceExtensionSupport(VkPhysicalDevice device, std::vector<const char *> requiredDeviceExtensions);

	SwapChainSupportDetails vhDevQuerySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);

	VkSurfaceFormatKHR vhDevChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);

	VkPresentModeKHR vhDevChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);

	VkExtent2D vhDevChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, int width, int height);

	VkResult vhDevCreateLogicalDevice(VkInstance instance, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface
		, std::vector<const char *> requiredDeviceExtensions, std::vector<const char *> requiredValidationLayers
		, void *pNextChain, VkDevice *device, VkQueue *graphicsQueue, VkQueue *presentQueue);

	






}
