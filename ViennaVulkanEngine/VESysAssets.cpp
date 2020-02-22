

#include "VEDefines.h"
#include "VETable.h"
#include "VESysEngine.h"
#include "VESysAssets.h"


namespace vve::sysass {

	VeVariableSizeTable* g_meshes_table = nullptr;

	void createTables() {
		g_meshes_table = new VeVariableSizeTable(1 << 20);
		syseng::registerTablePointer(g_meshes_table, "Meshes");

	}

	void init() {

	}

	void tick() {

	}

	void close() {

	}



}


