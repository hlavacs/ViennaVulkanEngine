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

	const std::string VE_SYSTEM_NAME = "VE SYSTEM WINDOW";


	std::vector<const char*> getRequiredInstanceExtensions() {
		return glfw::getRequiredInstanceExtensions();
	}

	bool createSurface(VkInstance instance, VkSurfaceKHR* pSurface) {
		return glfw::createSurface(instance, pSurface);
	}

	void init() {
		syseng::registerSystem(VE_SYSTEM_NAME);
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

