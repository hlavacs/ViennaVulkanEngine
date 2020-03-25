
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
#include "VESysAssets.h"


namespace vve::sysass {

	VeVariableSizeTableMT g_meshes_table("Meshes Table", 1 << 20);
	VeVariableSizeTableMT g_meshes_table2(g_meshes_table);


	void init() {
		syseng::registerEntity(VE_SYSTEM_NAME);
		VE_SYSTEM_HANDLE = syseng::getEntityHandle(VE_SYSTEM_NAME);

		syseng::registerTablePointer(&g_meshes_table);
	}

	void update() {
	}

	void cleanUp() {
	}

	void close() {
	}

}


