/**
*
* \file
* \brief
*
* Details
*
*/

#include "VEDefines.h"
#include "VESysEngine.h"
#include "VESysVulkanFWSimple.h"


namespace vve::sysvul::fwsimple {

	const std::string VE_SYSTEM_NAME = "VE SYSTEM VULKAN RENDERER FWSIMPLE";


	void init() {
		syseng::registerSystem(VE_SYSTEM_NAME);

		std::vector<const char*> device_extensions = { "VK_KHR_swapchain" };
		std::vector<const char*> device_layers = {};

	}

	void update() {

	}

	void cleanUp() {

	}

	void close() {

	}

}



