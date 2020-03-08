
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

	VeVariableSizeTableMT g_meshes_table(1 << 20);
	VeVariableSizeTableMT g_meshes_table2(g_meshes_table);


	void init() {
		syseng::registerTablePointer(&g_meshes_table, "Meshes Table");
	}

	void tick() {
	}

	void cleanUp() {
	}

	void close() {
	}

}


