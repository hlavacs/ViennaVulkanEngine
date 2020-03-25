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
#include "VESysWindow.h"
#include "VESysWindowGLFW.h"
#include "VESysVulkan.h"


namespace vve::syswin {


	std::vector<const char*> getRequiredInstanceExtensions() {
		return glfw::getRequiredInstanceExtensions();
	}

	bool createSurface(VkInstance instance, VkSurfaceKHR* pSurface) {
		return glfw::createSurface(instance, pSurface);
	}

	void init() {
		syseng::registerEntity(VE_SYSTEM_NAME);
		VE_SYSTEM_HANDLE = syseng::getEntityHandle(VE_SYSTEM_NAME);

		glfw::init();
	}

	void update() {
		glfw::update();
	}

	void cleanUp() {
		glfw::cleanUp();
	}

	void close() {
		glfw::close();
	}

	void windowSizeChanged() {
		sysvul::windowSizeChanged();
	}

}

