#include"VEInclude.h"
#include"VERendererPathTracing.h"

namespace ve {

	void VERendererPathTracing::initRenderer() {
		const std::vector<const char*> requiredDeviceExtensions = {
			VK_KHR_SWAPCHAIN_EXTENSION_NAME
		};

		const std::vector<const char*> requiredValidationLayers = {
			"VK_LAYER_LUNARG_standard_validation"
		};

		vh::vhDevPickPhysicalDevice(getEnginePointer()->getInstance(), m_surface, requiredDeviceExtensions, &m_physicalDevice);
		vh::vhDevCreateLogicalDevice(m_physicalDevice, m_surface, requiredDeviceExtensions, requiredValidationLayers,
			std::vector<uint32_t>{VK_QUEUE_COMPUTE_BIT, 0}, &m_device, std::vector<VkQueue*>{ &m_computeQueue, &m_presentQueue });

		vh::vhMemCreateVMAAllocator(m_physicalDevice, m_device, m_vmaAllocator);

		vh::vhSwapCreateSwapChain(m_physicalDevice, m_surface, m_device, getWindowPointer()->getExtent(),
			&m_swapChain, m_swapChainImages, m_swapChainImageViews,
			&m_swapChainImageFormat, &m_swapChainExtent);

	}
	
}