
#include "vulkan.h"

#include "VEDefines.h"
#include "VEDevice.h"


namespace vve::dev {



	//-------------------------------------------------------------------------------------------------------
/**
*
* \brief Create a Vulkan instance
*
* \param[in] extensions Requested layers
* \param[in] validationLayers Requested validation layers
* \param[out] instance The new instance
* \returns VK_SUCCESS or a Vulkan error code
*
*/
	VkResult vhDevCreateInstance(VeVector<const char*>& extensions, VeVector<const char*>& validationLayers, VkInstance *instance) {

		if (validationLayers.size() > 0 ) {   //&& !checkValidationLayerSupport(validationLayers)) {
			assert(false);
			exit(1);
		}

		VkApplicationInfo appInfo = {};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_0;

		VkInstanceCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;
		createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		createInfo.ppEnabledExtensionNames = extensions.data();

		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();

		return vkCreateInstance(&createInfo, nullptr, instance);
	}




}




