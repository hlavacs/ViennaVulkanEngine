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


	void init() {
		syseng::registerEntity(VE_SYSTEM_NAME);
		VE_SYSTEM_HANDLE = syseng::getEntityHandle(VE_SYSTEM_NAME);

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



