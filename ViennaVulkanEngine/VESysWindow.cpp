
#include "VEDefines.h"
#include "VESysEngine.h"
#include "VESysWindow.h"
#include "VESysWindowGLFW.h"


namespace vve::syswin {

	std::vector<const char*> getRequiredInstanceExtensions() {
		return glfw::getRequiredInstanceExtensions();
	}

	bool createSurface(VkInstance instance, VkSurfaceKHR* pSurface) {
		return glfw::createSurface(instance, pSurface);
	}

	void init() {
		glfw::init();
	}

	void tick() {
		glfw::tick();
	}

	void sync() {
		glfw::sync();
	}

	void close() {
		glfw::close();
	}



}

