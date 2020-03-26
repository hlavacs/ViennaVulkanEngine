/**
*
* \file
* \brief
*
* Details
*
*/


#include "VEDefines.h"
#include "VESysMessages.h"
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

		g_updateHandle = sysmes::addHandler(std::bind(update, std::placeholders::_1));
		sysmes::subscribeMessage(	syseng::VE_SYSTEM_HANDLE, VE_NULL_HANDLE, g_updateHandle,
								sysmes::VeMessageType::VE_MESSAGE_TYPE_UPDATE, 0);

		g_closeHandle = sysmes::addHandler(std::bind(close, std::placeholders::_1));
		sysmes::subscribeMessage(	VE_SYSTEM_HANDLE, VE_NULL_HANDLE, g_closeHandle,
								sysmes::VeMessageType::VE_MESSAGE_TYPE_CLOSE, 0);

		glfw::init();
	}

	void update(sysmes::VeMessageTableEntry e) {
		glfw::update(e);
	}

	void closeWin() {
		sysmes::addMessage({ sysmes::VeMessageType::VE_MESSAGE_TYPE_CLOSE, VE_SYSTEM_HANDLE });
	}

	void close(sysmes::VeMessageTableEntry e) {
		glfw::close(e);
	}

	void windowSizeChanged() {
		sysvul::windowSizeChanged();
	}

}

