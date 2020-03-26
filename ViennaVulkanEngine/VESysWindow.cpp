/**
*
* \file
* \brief
*
* Details
*
*/


#include "VEDefines.h"
#include "VESysEvents.h"
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

	VeHandle g_updateHandle;
	VeHandle g_closeHandle;

	void init() {
		syseng::registerEntity(VE_SYSTEM_NAME);
		VE_SYSTEM_HANDLE = syseng::getEntityHandle(VE_SYSTEM_NAME);

		g_updateHandle = syseve::addHandler(std::bind(update, std::placeholders::_1));
		syseve::subscribeEvent(	syseng::VE_SYSTEM_HANDLE, VE_NULL_HANDLE, g_updateHandle,
								syseve::VeEventType::VE_EVENT_TYPE_UPDATE, 0);

		g_closeHandle = syseve::addHandler(std::bind(close, std::placeholders::_1));
		syseve::subscribeEvent(	VE_SYSTEM_HANDLE, VE_NULL_HANDLE, g_closeHandle,
								syseve::VeEventType::VE_EVENT_TYPE_CLOSE, 0);

		glfw::init();
	}

	void update(syseve::VeEventTableEntry e) {
		glfw::update(e);
	}

	void closeWin() {
		syseve::addEvent({ syseve::VeEventType::VE_EVENT_TYPE_CLOSE, VE_SYSTEM_HANDLE });
	}

	void close(syseve::VeEventTableEntry e) {
		glfw::close(e);
	}

	void windowSizeChanged() {
		sysvul::windowSizeChanged();
	}

}

