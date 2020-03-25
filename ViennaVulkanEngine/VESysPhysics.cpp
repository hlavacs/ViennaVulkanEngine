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
#include "VESysEvents.h"
#include "VESysPhysics.h"

namespace vve::sysphy {


	void init() {
		syseng::registerEntity(VE_SYSTEM_NAME);
		VE_SYSTEM_HANDLE = syseng::getEntityHandle(VE_SYSTEM_NAME);

	}

	void update() {

	}

	void cleanUp() {
	}



	void close() {

	}





}