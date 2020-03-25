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
#include "VESysPhysics.h"

namespace vve::sysphy {

	const std::string VE_SYSTEM_NAME = "VE SYSTEM PHYSICS";

	void init() {
		syseng::registerSystem(VE_SYSTEM_NAME);
	}

	void update() {

	}

	void cleanUp() {
	}



	void close() {

	}





}