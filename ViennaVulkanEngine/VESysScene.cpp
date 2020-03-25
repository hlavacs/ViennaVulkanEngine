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
#include "VESysScene.h"


namespace vve::syssce {

	const std::string VE_SYSTEM_NAME = "VE SYSTEM SCENE";


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


