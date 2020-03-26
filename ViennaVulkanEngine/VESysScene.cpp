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
#include "VESysScene.h"


namespace vve::syssce {

	VeHandle g_updateHandle;
	VeHandle g_closeHandle;

	void init() {
		syseng::registerEntity(VE_SYSTEM_NAME);
		VE_SYSTEM_HANDLE = syseng::getEntityHandle(VE_SYSTEM_NAME);

		g_updateHandle = syseve::addHandler(std::bind(update, std::placeholders::_1));
		syseve::subscribeEvent(	syseng::VE_SYSTEM_HANDLE, VE_NULL_HANDLE, g_updateHandle,
								syseve::VeEventType::VE_EVENT_TYPE_UPDATE);

		g_closeHandle = syseve::addHandler(std::bind(close, std::placeholders::_1));
		syseve::subscribeEvent(	syswin::VE_SYSTEM_HANDLE, VE_NULL_HANDLE, g_closeHandle,
								syseve::VeEventType::VE_EVENT_TYPE_CLOSE);


	}

	void update(syseve::VeEventTableEntry e) {

	}

	void close(syseve::VeEventTableEntry e) {

	}


}


